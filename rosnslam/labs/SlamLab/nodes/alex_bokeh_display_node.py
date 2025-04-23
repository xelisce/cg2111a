# This node displays the raw LIDAR scan data and SLAM map in real-time using matplotlib figures.
# It subscribes to the "lidar/scan" and "slam/mappose" topics to receive LIDAR scan data
# and SLAM map updates, respectively.

# from multiprocessing import Barrier  # We use threading.Barrier in code, but the usage is similar
from threading import Barrier

# PubSub import
from pubsub.pub_sub_manager import ManagedPubSubRunnable, PubSubMsg
from pubsub.pub_sub_manager import publish, subscribe, unsubscribe, getMessages, getCurrentExecutionContext

# Lidar / SLAM / Display utilities
from lidar.alex_lidar import resampleLidarScan
from slam.alex_slam import mapBytesToGrid
from display.alex_display_utilities import projectCoordinates, rotateAboutOrigin, getDelta

import numpy as np
import time

# Bokeh Imports
from bokeh.plotting import figure, curdoc
from bokeh.models import ColumnDataSource
from bokeh.server.server import Server
from bokeh.application import Application
from bokeh.application.handlers.function import FunctionHandler
from bokeh.layouts import row

# Hack to enable compression in Bokeh WebSocket handler
# (you can comment out these lines if you do NOT want compression)
from bokeh.server.views.ws import WSHandler
import bokeh.server.views.ws
class CompressionEnabledWSHandler(WSHandler):
    def get_compression_options(self):
        return {"compression_level": 9}

bokeh.server.views.ws.WSHandler = CompressionEnabledWSHandler

###########################
### Constants and Topics ##
###########################
ARDUINO_SEND_TOPIC = "arduino/send"
SLAM_MAPPOSE_TOPIC = "slam/mappose"
LIDAR_SCAN_TOPIC    = "lidar/scan"

# Plotting Constants
FRAMERATE             = 60
LIDAR_OFFSET_DEGREES  = 0  # shift if your LIDAR is rotated
LIDAR_OFFSET_RADIANS  = np.deg2rad(LIDAR_OFFSET_DEGREES)

# SLAM / MAP constants
MAP_SIZE_PIXELS = 500
MAP_SIZE_METERS  = 8
MAP_SIZE_MILLIMETERS = MAP_SIZE_METERS * 1000

ROBOT_WIDTH_METERS  = 0.2
ROBOT_HEIGHT_METERS = 0.2

# We don't need these for the dot+line approach, but we'll keep them for reference
ROBOT_HALF_HEIGHT_MM = ROBOT_HEIGHT_METERS * 1000 / 2
ROBOT_HALF_WIDTH_MM  = ROBOT_WIDTH_METERS  * 1000 / 2
ROBOT_HALF_DIAGONAL  = np.sqrt(ROBOT_HALF_HEIGHT_MM**2 + ROBOT_HALF_WIDTH_MM**2)

IMAGE_MAP_DOWNSCALE_FACTOR  = 1
SLAM_MAP_GUI_UPDATE_INTERVAL = 0.25  # seconds between map updates

###############################
### New Helper for Dot+Line ###
###############################
def makeRobotDotAndGripperLine(robotXmm, robotYmm, robotThetaDegrees=0):
    """
    Returns arrays for:
      - a small dot (robot center)
      - a line representing the grippers/heading
   
    robotXmm, robotYmm: Robot center in mm
    robotThetaDegrees: Robot heading in degrees from 'north'
    """

    # Dot is just one point
    dot_x = [robotXmm]
    dot_y = [robotYmm]

    # We'll draw a short line out in front to simulate grippers
    line_length = 220  # 100 mm in front of the dot
    # Convert heading: 'north=0' => shift by -90 for standard math x+ axis
    angle_rad = np.deg2rad(-robotThetaDegrees + 90)
    line_end_x = robotXmm + line_length * np.cos(angle_rad)
    line_end_y = robotYmm + line_length * np.sin(angle_rad)

    line_x = [robotXmm, line_end_x]
    line_y = [robotYmm, line_end_y]

    return dot_x, dot_y, line_x, line_y

###########################
### Main Display Process ##
###########################
def lidarDisplayProcess(setupBarrier: Barrier = None, readyBarrier: Barrier = None):
    """
    Initializes and runs the Lidar Display Process.
    This function sets up and displays live LIDAR scan data and the SLAM map.

    Args:
        setupBarrier (Barrier, optional): A threading barrier used for initial setup synchronization.
        readyBarrier (Barrier, optional): A threading barrier to synchronize the start of the display process
                                          with other threads.
    """
    ctx: ManagedPubSubRunnable = getCurrentExecutionContext()

    # Barrier usage
    setupBarrier.wait() if setupBarrier else None

    # Subscriptions
    subscribe(topic=LIDAR_SCAN_TOPIC, ensureReply=True, replyTimeout=1)
    subscribe(topic=SLAM_MAPPOSE_TOPIC, ensureReply=True, replyTimeout=1)

    # Create Bokeh plots
    datasources = {}
    lidarPlot, lidarDs = createLidarPlot()
    datasources["lidarscan"] = lidarDs

    slamPlot, imageDs, dotDs, lineDs = createSlamPlot()
    datasources["slam"] = {
        "image": imageDs,
        "dot": dotDs,
        "line": lineDs
    }

    overallPlot = createLayout([lidarPlot, slamPlot])

    # Print some details
    paramsStr = f"Display Parameters:\n"
    paramsStr += f"Lidar Offset (Degrees): {LIDAR_OFFSET_DEGREES}\n"
    paramsStr += f"Map Size (Pixels): {MAP_SIZE_PIXELS}\n"
    paramsStr += f"Map Size (Meters): {MAP_SIZE_METERS}\n"
    paramsStr += f"Robot Width (Meters): {ROBOT_WIDTH_METERS}\n"
    paramsStr += f"Robot Height (Meters): {ROBOT_HEIGHT_METERS}\n"
    paramsStr += f"Max Framerate: {FRAMERATE}\n"
    print(paramsStr)

    # Set up Bokeh server
    serv = setupBokehServer(overallPlot, datasources)

    # Barrier usage
    readyBarrier.wait() if readyBarrier else None

    try:
        runBokehServer(serv)  # Blocks until server stops
    except KeyboardInterrupt:
        pass
    except Exception as e:
        print(f"Lidar Display Process Exception: {e}")
        pass

    # Exit
    ctx.doExit()
    print("Exiting Lidar Display Process")


##########################
### Utility Conversions ##
##########################
def polarToCartesian(angles, distances, cardinalZero="N"):
    """
    Convert polar coordinates to Cartesian coordinates.

    Args:
        angles: array-like of angles in degrees
        distances: array-like of distances in mm
        cardinalZero: "N", "E", "S", or "W" defining your 0-degree direction
    Returns:
        x, y arrays (same length as angles/distances)
    """
    angles = -np.array(angles)  # Invert angles to match your orientation
    distances = np.array(distances)

    if cardinalZero == "N":
        angles = angles + 90
    elif cardinalZero == "E":
        angles = angles + 0
    elif cardinalZero == "S":
        angles = angles - 90
    elif cardinalZero == "W":
        angles = angles - 180

    angleRad = np.deg2rad(angles)
    x = distances * np.cos(angleRad)
    y = distances * np.sin(angleRad)
    return x, y


###################################
### Plot Creation / Layout Func ###
###################################
def createLayout(plots):
    """
    Arrange the provided Bokeh plots in a horizontal row.
    """
    layout = row(*plots)
    return layout


def createLidarPlot():
    """
    Create a Bokeh plot to display the Lidar scan data.
    Returns a tuple (figure, ColumnDataSource).
    """
    p = figure(title="Lidar Scan", x_axis_label='X (mm)', y_axis_label='Y (mm)',
               width=800, height=800)

    # The Lidar data source
    source = ColumnDataSource(data=dict(x=[], y=[]))

    # Lidar scatter
    p.scatter(x='x', y='y', source=source,
              size=3, color="red", alpha=1, legend_label="Lidar Scan", marker="circle")

    # Axes config
    p.xaxis.axis_label = "X (mm)"
    p.yaxis.axis_label = "Y (mm)"
    p.xaxis.axis_label_standoff = 10

    # Set axis ranges based on half map size
    p.x_range.start = -MAP_SIZE_MILLIMETERS / 2
    p.x_range.end   = +MAP_SIZE_MILLIMETERS / 2
    p.y_range.start = -MAP_SIZE_MILLIMETERS / 2
    p.y_range.end   = +MAP_SIZE_MILLIMETERS / 2

    # Remove ticks, lines
    p.xaxis.ticker = []
    p.yaxis.ticker = []
    p.xaxis.axis_line_color = None
    p.yaxis.axis_line_color = None

    # --- Robot marker: a dot + short line at the origin ---
    dot_x, dot_y, line_x, line_y = makeRobotDotAndGripperLine(0, 0, 0)

    # Center dot
    # p.circle(x=dot_x, y=dot_y,
             # size=8,
             # color="red",
             # alpha=1,
             # legend_label="Robot Center")

    p.rect(x=dot_x, y=dot_y,
           width=180,
           height=230,
           fill_color='red',
           alpha=1,
           legend_label="Robot Center",
           )

    # Heading line
    p.line(x=line_x, y=line_y,
           line_width=2,
           color="blue",
           alpha=1,
           legend_label="Robot Heading")

    # Draw distance rings (every 500 mm)
    firstRing_radius = 500
    ring_interval    = 500
    max_distance     = 10001
    for i in range(firstRing_radius, max_distance, ring_interval):
        p.circle(x=0, y=0, radius=i,
                 fill_color=None, line_color="black", line_width=0.5, alpha=0.5)
        # Label each ring at 45 degrees
        angle = np.deg2rad(45)
        x_anno = i * np.cos(angle)
        y_anno = i * np.sin(angle)
        x_offset = 10 * np.cos(angle)
        y_offset = 10 * np.sin(angle)
        p.text(x=x_anno + x_offset, y=y_anno + y_offset,
               text=[f"{i} mm"],
               text_color="black", text_font_size="10pt")

    return p, source


def createSlamPlot():
    p = figure(title="SLAM Map", x_axis_label='X (M)', y_axis_label='Y (M)',
               width=800, height=800)

    # Map image source
    imageSource = ColumnDataSource(data=dict(image=[]))

    # Dot source (always length=1 for the center)
    poseDotSource = ColumnDataSource(data=dict(x=[], y=[]))

    # Line source (always length=2 for the front line)
    poseLineSource = ColumnDataSource(data=dict(x=[], y=[]))

    # Draw the map image
    map_zero_x = 0
    map_zero_y = 0
    map_w_mm = MAP_SIZE_METERS * 1000
    map_h_mm = MAP_SIZE_METERS * 1000

    p.image(image='image',
            x=map_zero_x, y=map_zero_y,
            dw=map_w_mm, dh=map_h_mm,
            source=imageSource, palette="Greys256")

    # Draw the center as a dot
    p.circle(x='x', y='y', size=12, color='red', alpha=1, source=poseDotSource)

    # Draw the robot’s heading/gripper line
    p.line(x='x', y='y', line_width=2, color='red', alpha=1, source=poseLineSource)

    return p, imageSource, poseDotSource, poseLineSource


######################
### Update Routines ##
######################
def updateLidarPlot(message, datasources):
    """
    Update the Lidar plot with new scan data from a PubSub message.
    """
    angleData, distanceData, qualityData = PubSubMsg.getPayload(message)

    goodQuality = np.array(qualityData) > 100
    angleData   = np.array(angleData)[goodQuality]
    distanceData= np.array(distanceData)[goodQuality]

    offset_degrees = LIDAR_OFFSET_DEGREES
    target_measurements_per_scan = 180
    merge_strategy = np.mean
    fill_value = 99999

    dist, angle = resampleLidarScan(
        distance=distanceData,
        angles=angleData,
        target_measurements_per_scan=target_measurements_per_scan,
        offset_degrees=offset_degrees,
        merge_strategy=merge_strategy,
        fill_value=fill_value
    )

    x, y = polarToCartesian(angle, dist)
    rollover = len(x)
    datasources["lidarscan"].stream({'x': x, 'y': y}, rollover=rollover)


lastUpdateSlamPicture = [0]  # hacky static container
def updateSlamPlot(message, datasources):
    x_mm, y_mm, thetaDeg, mapbytes = PubSubMsg.getPayload(message)

    # Convert map to grid, etc.
    grid = mapBytesToGrid(mapbytes, MAP_SIZE_PIXELS, MAP_SIZE_PIXELS)

    # Dot + line arrays
    dot_x, dot_y, line_x, line_y = makeRobotDotAndGripperLine(x_mm, y_mm, thetaDeg)

    # If it's time to refresh the map
    currentTime = time.time()
    if (currentTime - lastUpdateSlamPicture[0]) > SLAM_MAP_GUI_UPDATE_INTERVAL:
        lastUpdateSlamPicture[0] = currentTime
        datasources["slam"]["image"].data = dict(image=[grid])

    # Update the dot (single point)
    datasources["slam"]["dot"].data = dict(x=dot_x, y=dot_y)
    # Update the line (two points)
    datasources["slam"]["line"].data = dict(x=line_x, y=line_y)


def updatePlots(datasources):
    """
    Process incoming PubSub messages and update Bokeh plots accordingly.
    """
    pubSubMessages = getMessages(block=True, timeout=1)
    updates = {x:False for x in datasources.keys()}

    # We only want to apply one update per topic on each refresh
    for m in reversed(pubSubMessages):
        m_topic = PubSubMsg.getTopic(m)

        if (m_topic == LIDAR_SCAN_TOPIC) and (not updates["lidarscan"]):
            updateLidarPlot(m, datasources)
            updates["lidarscan"] = True

        elif (m_topic == SLAM_MAPPOSE_TOPIC) and (not updates["slam"]):
            updateSlamPlot(m, datasources)
            updates["slam"] = True

        if all(updates.values()):
            break


######################
### Bokeh Setup    ###
######################
def setupBokehServer(bokehPlot, datasources):
    """
    Set up the Bokeh server with a callback to update the plots.
    """
    def initPlots(doc):
        doc.add_root(bokehPlot)
        doc.add_periodic_callback(lambda: updatePlots(datasources), 1000 / FRAMERATE)

    app = Application(FunctionHandler(initPlots))
    ip = "0.0.0.0"
    port = 8181
    server = Server(
        {'/': app},
        address=ip,
        port=port,
        allow_websocket_origin=[f"*:{port}"],
    )
    server.start()
    print(f"Bokeh server is running at http://{ip}:{port}/")
    if ip == "0.0.0.0":
        print(f"{ip} means all interfaces. Access it from your host machine using the IP of your Pi.")
    return server


def runBokehServer(server):
    """
    Start the Bokeh server I/O loop. This call blocks until the server stops.
    """
    server.io_loop.start()
