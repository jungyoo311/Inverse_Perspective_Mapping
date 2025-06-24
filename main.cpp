#include <opencv2/opencv.hpp>
#include <iostream>
/// V1 with simple implementation
/// 06/24/2025
using namespace cv;
using namespace std;

// Function to perform Inverse Perspective Mapping
Mat IPM(const Mat& image) {
    int height = image.rows;
    int width = image.cols;
    
    // Parameters (hard-coded as in original Python code)
    int param1 = 570;
    int param2 = 35;
    
    // Define source points for perspective transformation
    vector<Point2f> original_points = {
        Point2f(0, (height / 2) + param2),          // Top-left of the lower half
        Point2f(width, (height / 2) + param2),      // Top-right of the lower half
        Point2f(width, height),                     // Bottom-right corner
        Point2f(0, height)                          // Bottom-left corner
    };
    
    // Define destination points for perspective transformation
    vector<Point2f> destination_points = {
        Point2f(0, 0),                              // Top-left corner
        Point2f(width, 0),                          // Top-right corner
        Point2f(width - param1, height * 2),        // Bottom-right corner
        Point2f(param1, height * 2)                 // Bottom-left corner
    };
    
    // Compute and apply the perspective transformation
    Mat matrix = getPerspectiveTransform(original_points, destination_points);
    Mat warped_image;
    warpPerspective(image, warped_image, matrix, Size(width, height * 2));
    
    // Resize back to original dimensions
    Mat final_warped_image;
    resize(warped_image, final_warped_image, Size(width, height));
    
    return final_warped_image;
}

// Function to create picture-in-picture overlay
Mat pictureInPicture(Mat main_image, const Mat& overlay_image, 
                    int img_ratio = 3, int border_size = 3, 
                    int x_margin = 30, int y_offset_adjust = -100) {
    
    if (main_image.empty() || overlay_image.empty()) {
        throw runtime_error("One or both images are empty.");
    }
    
    // Resize the overlay image to 1/img_ratio of the main image height
    int new_height = main_image.rows / img_ratio;
    int new_width = static_cast<int>(new_height * (static_cast<double>(overlay_image.cols) / overlay_image.rows));
    
    Mat overlay_resized;
    resize(overlay_image, overlay_resized, Size(new_width, new_height));
    
    // Add a white border to the overlay image
    Mat overlay_with_border;
    copyMakeBorder(overlay_resized, overlay_with_border, 
                   border_size, border_size, border_size, border_size,
                   BORDER_CONSTANT, Scalar(255, 255, 255));
    
    // Determine overlay position
    int x_offset = main_image.cols - overlay_with_border.cols - x_margin;
    int y_offset = (main_image.rows / 2) - overlay_with_border.rows + y_offset_adjust;
    
    // Ensure the overlay fits within the main image bounds
    if (x_offset >= 0 && y_offset >= 0 && 
        x_offset + overlay_with_border.cols <= main_image.cols &&
        y_offset + overlay_with_border.rows <= main_image.rows) {
        
        // Create ROI and copy overlay
        Rect roi(x_offset, y_offset, overlay_with_border.cols, overlay_with_border.rows);
        overlay_with_border.copyTo(main_image(roi));
    }
    
    return main_image;
}

int main() {
    // Input and output file paths
    string input_video_path = "../video1.hevc";
    string output_video_path = "carla_BEV_IPM_output_.mp4";
    
    // Open the input video
    VideoCapture cap(input_video_path);
    
    // Check if video opened successfully
    if (!cap.isOpened()) {
        cout << "Error: Unable to open video file." << endl;
        return -1;
    }
    
    // Get video properties
    int frame_width = 1280;
    int frame_height = 800;
    double fps = cap.get(CAP_PROP_FPS);
    
    // Create VideoWriter object
    VideoWriter out(output_video_path, VideoWriter::fourcc('m', 'p', '4', 'v'), 
                    fps, Size(frame_width, frame_height));
    
    if (!out.isOpened()) {
        cout << "Error: Unable to create output video file." << endl;
        return -1;
    }
    
    Mat frame, frame_imp;
    
    // Process the video
    while (true) {
        bool ret = cap.read(frame);
        if (!ret) {
            cout << "End of video or error reading frame." << endl;
            break;
        }
        
        // Resize frame to desired dimensions
        resize(frame, frame, Size(frame_width, frame_height));
        
        // Apply IPM transformation
        frame_imp = IPM(frame);
        
        // Apply picture-in-picture overlay
        frame = pictureInPicture(frame, frame_imp);
        
        // For side-by-side instead of PIP, uncomment the following lines:
        // Mat frame_imp_resized;
        // resize(frame_imp, frame_imp_resized, Size(frame_width/2, frame_height));
        // Mat combined;
        // hconcat(frame, frame_imp_resized, combined);
        // frame = combined;
        
        // Display the frame
        imshow("Frame", frame);
        
        // Ensure frame is correct size before writing
        Mat output_frame;
        resize(frame, output_frame, Size(frame_width, frame_height));
        out.write(output_frame);
        
        // Press 'q' to exit the display window
        if (waitKey(1) == 'q') {
            break;
        }
    }
    
    // Release video objects and close windows
    cap.release();
    out.release();
    destroyAllWindows();
    
    cout << "Video saved as: " << output_video_path << endl;
    
    return 0;
}