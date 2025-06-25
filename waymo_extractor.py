#!/usr/bin/env python3
# Image Extraction
# Used for waymo-open-dataset percpetion dataset v.1.4.3 March 2024 released
# June 2025
# Copyright by github.com/jungyoo311

# Example usage commands:
"""
# Preview what's in the file
python waymo_extractor.py --tfrecord assets/segment-1063302682054393085_705_000_725_000.tfrecord --output_dir ./output --preview

# Extract front camera images (good for IPM)
python waymo_extractor.py --tfrecord assets/segment-10495858009395654700_197_000_217_000.tfrecord --output_dir ./output --camera FRONT

# Extract first 50 frames only for testing
python waymo_extractor.py --tfrecord segment-10330268205439308_705_000_725_000.tfrecord --output_dir ./output --camera FRONT --max_frames 50

# Extract all cameras - default
python waymo_extractor.py --tfrecord assets/segment-10495858009395654700_197_000_217_000.tfrecord --output_dir ./output --camera ALL
"""
"""
Simplified Waymo Image Extractor based on the official tutorial
Extracts camera images from Waymo .tfrecord files for IPM processing
"""

import tensorflow as tf
import os
import numpy as np
from PIL import Image
import matplotlib.pyplot as plt
import argparse
from tqdm import tqdm

# Enable eager execution (for older TF versions)
try:
    tf.enable_eager_execution()
except:
    pass  # Already enabled in TF 2.x

from waymo_open_dataset import dataset_pb2 as open_dataset

def extract_images_from_single_tfrecord(tfrecord_path, output_base_dir, camera_name='FRONT', max_frames=None):
    """
    Extract images from a single tfrecord file using the tutorial approach
    
    Args:
        tfrecord_path: Path to the .tfrecord file
        output_dir: Directory to save extracted images
        camera_name: Which camera to extract ('FRONT', 'FRONT_LEFT', 'FRONT_RIGHT', 'SIDE_LEFT', 'SIDE_RIGHT')
        max_frames: Maximum number of frames to process (None for all)
    """
    
    # Create output directory
    camera_output_dir = os.path.join(output_base_dir, camera_name.lower())
    os.makedirs(camera_output_dir, exist_ok=True)
    
    # Camera name mapping
    camera_mapping = {
        'FRONT': open_dataset.CameraName.FRONT,
        'FRONT_LEFT': open_dataset.CameraName.FRONT_LEFT,
        'FRONT_RIGHT': open_dataset.CameraName.FRONT_RIGHT,
        'SIDE_LEFT': open_dataset.CameraName.SIDE_LEFT,
        'SIDE_RIGHT': open_dataset.CameraName.SIDE_RIGHT
    }
    
    if camera_name not in camera_mapping:
        raise ValueError(f"Invalid camera name. Choose from: {list(camera_mapping.keys())}")
    
    target_camera = camera_mapping[camera_name]
    
    # Read the tfrecord file (following tutorial format)
    dataset = tf.data.TFRecordDataset(tfrecord_path, compression_type='')
    
    frame_count = 0
    saved_count = 0
    
    print(f"Processing {os.path.basename(tfrecord_path)}")
    print(f"Extracting {camera_name} camera images to: {camera_output_dir}")
    
    for data in tqdm(dataset):
        if max_frames and frame_count >= max_frames:
            break
            
        # Parse frame (following tutorial approach)
        frame = open_dataset.Frame()
        frame.ParseFromString(bytearray(data.numpy()))
        
        # Extract images from the frame
        for image in frame.images:
            if image.name == target_camera:
                # Decode the image using TensorFlow (as in tutorial)
                img_tensor = tf.image.decode_jpeg(image.image)
                img_array = img_tensor.numpy()
                
                # Convert to PIL Image and save
                img = Image.fromarray(img_array)
                
                # Create filename with frame timestamp for uniqueness
                timestamp = frame.timestamp_micros
                filename = f"frame_{frame_count:06d}_{timestamp}_{camera_name.lower()}.jpg"
                img_path = os.path.join(camera_output_dir, filename)
                
                img.save(img_path, quality=95)
                saved_count += 1
                
                # Print image info for first few frames
                if frame_count < 3:
                    print(f"  Frame {frame_count}: {img_array.shape} -> {camera_name.lower()}/{filename}")
                
                break
        
        frame_count += 1
    
    print(f"Processed {frame_count} frames, saved {saved_count} images to {camera_output_dir}")
    return saved_count

def batch_extract_all_cameras(tfrecord_path, output_base_dir, max_frames=None):
    """
    Extract images from all cameras in a tfrecord file
    """
    cameras = ['FRONT', 'FRONT_LEFT', 'FRONT_RIGHT', 'SIDE_LEFT', 'SIDE_RIGHT']
    
    for camera in cameras:
        print(f"\n--- Processing {camera} camera ---")
        output_dir = os.path.join(output_base_dir, camera.lower())
        try:
            extract_images_from_single_tfrecord(tfrecord_path, output_dir, camera, max_frames)
        except Exception as e:
            print(f"Error processing {camera}: {str(e)}")
            continue

def display_sample_images(tfrecord_path, max_display=1):
    """
    Display sample images from the tfrecord file (following tutorial visualization)
    """
    dataset = tf.data.TFRecordDataset(tfrecord_path, compression_type='')
    
    frame_count = 0
    for data in dataset:
        if frame_count >= max_display:
            break
            
        frame = open_dataset.Frame()
        frame.ParseFromString(bytearray(data.numpy()))
        
        print(f"\nFrame {frame_count} info:")
        print(f"Timestamp: {frame.timestamp_micros}")
        print(f"Number of cameras: {len(frame.images)}")
        
        # Display all camera images in this frame
        plt.figure(figsize=(20, 12))
        for index, image in enumerate(frame.images):
            camera_name = open_dataset.CameraName.Name.Name(image.name)
            print(f"  Camera {index + 1}: {camera_name}")
            
            # Decode and display
            img_tensor = tf.image.decode_jpeg(image.image)
            
            plt.subplot(2, 3, index + 1)
            plt.imshow(img_tensor)
            plt.title(f"{camera_name}")
            plt.axis('off')
        
        plt.tight_layout()
        plt.show()
        frame_count += 1

def main():
    parser = argparse.ArgumentParser(description='Extract images from Waymo tfrecord files (Tutorial-based)')
    parser.add_argument('--tfrecord', type=str, required=True,
                       help='Path to .tfrecord file')
    parser.add_argument('--output_dir', type=str, required=True,
                       help='Output directory for extracted images')
    parser.add_argument('--camera', type=str, default='FRONT',
                       choices=['FRONT', 'FRONT_LEFT', 'FRONT_RIGHT', 'SIDE_LEFT', 'SIDE_RIGHT', 'ALL'],
                       help='Camera to extract images from (or ALL for all cameras)')
    parser.add_argument('--max_frames', type=int, default=None,
                       help='Maximum number of frames to process')
    parser.add_argument('--preview', action='store_true',
                       help='Show preview of images before extraction')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.tfrecord):
        print(f"Error: File {args.tfrecord} does not exist")
        return
    
    if args.preview:
        print("Showing preview...")
        display_sample_images(args.tfrecord, max_display=1)
        return
    
    if args.camera == 'ALL':
        batch_extract_all_cameras(args.tfrecord, args.output_dir, args.max_frames)
    else:
        extract_images_from_single_tfrecord(args.tfrecord, args.output_dir, args.camera, args.max_frames)

if __name__ == "__main__":
    main()

