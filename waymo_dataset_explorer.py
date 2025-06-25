import tensorflow as tf
from waymo_open_dataset import dataset_pb2 as open_dataset

# Path to a downloaded segment
filename = "path/segment-14262528452308149004_549_000_569_000.tfrecord"

dataset = tf.data.TFRecordDataset(filename, compression_type='')
for data in dataset.take(1):  # Just read the first frame
    frame = open_dataset.Frame()
    frame.ParseFromString(bytearray(data.numpy()))

    print("Available data in this frame:")
    print("- Images:", [camera.name for camera in frame.images])
    print("- LIDARs:", [lidar.name for lidar in frame.lasers])
    print("- IMU:", "Present" if frame.pose else "Not Present")
    print("- GPS Pose:", "Present" if frame.pose else "Not Present")
    print("- Timestamp:", frame.timestamp_micros)
