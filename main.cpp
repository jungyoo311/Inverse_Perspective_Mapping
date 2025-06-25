#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include "Logger.h"
/// V2: logging completed
/// 06/24/2025
// ffmpeg -framerate 30 -pattern_type glob -i "./output/front/front/*.jpg" -s 1280x800 -c:v libx264 -crf 23 -pix_fmt yuv420p output_front.mp4
using namespace cv;
using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

Logger* g_logger = nullptr;
class PerformanceTracker {
private:
    int frame_count;
    double total_processing_time;
    double total_ipm_time;
    double total_pip_time;
    high_resolution_clock::time_point last_fps_time;
public:
    PerformanceTracker() : frame_count(0), total_processing_time(0), total_ipm_time(0), total_pip_time(0){
        last_fps_time = high_resolution_clock::now();
    }
    void updateFrameStats(double processing_time, double ipm_time, double pip_time){
        frame_count++;
        total_processing_time += processing_time;
        total_ipm_time += ipm_time;
        total_pip_time += pip_time;
        // Log FPS every 30 seconds
        if(frame_count % 30 == 0){
            auto current_time = high_resolution_clock::now();
            auto time_diff = duration_cast<milliseconds>(current_time - last_fps_time);
            double fps = 30000.0 / time_diff.count(); // 30 frames * 1000ms

            if (g_logger){
                g_logger->logFrameRate(fps);
                g_logger->logPerformance("Avg Processing Time", total_processing_time / frame_count);
                g_logger->logPerformance("Avg IPM Time", total_ipm_time / frame_count);
                g_logger->logPerformance("Avg PIP Time", total_pip_time / frame_count);
            }
            last_fps_time = current_time;
        }
    }
    void logSummary(){
        if(g_logger && frame_count > 0){
            LOG_INFO("=== Performance Summary ===");
            g_logger->logPerformance("Total Frames Processed", frame_count, " frames");
            g_logger->logPerformance("Average Processing Time", total_processing_time / frame_count);
            g_logger->logPerformance("Average IPM Time", total_ipm_time / frame_count);
            g_logger->logPerformance("Average PIP Time", total_pip_time / frame_count);
        }
    }
};

// Function to perform Inverse Perspective Mapping
Mat IPM(const Mat& image) {
    auto start_time = high_resolution_clock::now();

    int height = image.rows;
    int width = image.cols;
    LOG_DEBUG("IPM: Processing frame " + to_string(width) + "x" + to_string(height));

    // Parameters (hard-coded need to be fixed)
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
    try {
        // Compute and apply the perspective transformation
        Mat matrix = getPerspectiveTransform(original_points, destination_points);
        Mat warped_image;
        warpPerspective(image, warped_image, matrix, Size(width, height * 2));
        
        // Resize back to original dimensions
        Mat final_warped_image;
        resize(warped_image, final_warped_image, Size(width, height));

        auto end_time = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end_time - start_time);
        double ms = duration.count() / 1000.0;
        
        // Log performances
        if (ms > 10.0){
            LOG_WARNING("IPM processing slow: " + to_string(ms) + "ms");
        }
        return final_warped_image;
        
    } catch(const exception& e){
        LOG_ERROR("IPM failed: " + string(e.what()));
        return image; // return original image on failure
    }
}

// Function to create picture-in-picture overlay
Mat pictureInPicture(Mat main_image, const Mat& overlay_image, 
                    int img_ratio = 3, int border_size = 3, 
                    int x_margin = 30, int y_offset_adjust = -100) {
    
    if (main_image.empty() || overlay_image.empty()) {
        LOG_ERROR("PIP: One or both images are empty");
        return main_image; // is this necessary?
    }
    try {
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
    } catch(const exception& e){
        LOG_ERROR("PIP failed: " + string(e.what()));
        return main_image;
    }
}
//Get list of image files from diretory
vector<string> getImageFiles(const string& directory_path){
    vector<string> valid_extensions = {".jpg", ".jpeg", ".png"};
    vector<string> image_files;

    try {
        for (const auto& entry : fs::directory_iterator(directory_path)){
            if(entry.is_regular_file()){
                string file_path = entry.path().string();
                string extension = entry.path().extension().string();
                // convert extension to lowercase for comparison
                transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                // check if file has valid image extension
                if (find(valid_extensions.begin(), valid_extensions.end(), extension) != valid_extensions.end()){
                    image_files.push_back(file_path);
                }
            }
        }
        sort(image_files.begin(), image_files.end());
        LOG_INFO("Found " + to_string(image_files.size()) + " image files in directory: " + directory_path);

    } catch(const exception& e){
        LOG_ERROR("Error reading directory" + directory_path + ": " + string(e.what()));
    }
    return image_files;
}
int processImageSequence(const string& input_dir, const string& output_video_path, double fps =30.0, int frame_width = 1280, int frame_height = 800){
    LOG_INFO("=== Image Sequence Processing Started ===");
    LOG_INFO("Input Directory: " + input_dir);
    LOG_INFO("Output Video: " + output_video_path);

    // Get list of image files
    vector<string> image_files = getImageFiles(input_dir);
    if (image_files.empty()){
        LOG_ERROR("No valid image files found in directory: " + input_dir);
        return -1;
    }
    // Perf Tracker
    PerformanceTracker perf_tracker;
    // Create VideoWriter object
    VideoWriter out(output_video_path, VideoWriter::fourcc('m', 'p', '4', 'v'), fps, Size(frame_width, frame_height));
    if (!out.isOpened()){
        LOG_ERROR("Unable to create output video file: " + output_video_path);
        return -1;
    }
    LOG_INFO("Video writer initialized successfully");
    LOG_INFO("Processing " + to_string(image_files.size()) + " images at " + to_string(fps) + " fps");
    Mat frame, frame_ipm;
    int frame_number = 0;
    auto total_start_time = high_resolution_clock::now();
    // Process each image
    for (const string& image_path : image_files){
        auto frame_start_time = high_resolution_clock::now();
        frame_number++;

        // Log progress every 100 frames
        if (frame_number % 100 == 0){
            LOG_INFO("Processing image " + to_string(frame_number) + "/" + to_string(image_files.size()) + " (" + to_string((frame_number * 100) / image_files.size()) + "%)");
        }
        try {
            // read image
            frame = imread(image_path);
            if (frame.empty()){
                LOG_WARNING("Failed to read image: " + image_path + " - skipping");
                continue;
            }
            // Resize frame to desired dimensions
            resize(frame, frame, Size(frame_width, frame_height));
            // Apply IPM transformation with timing
            PERF_START("IPM Transform");
            auto ipm_start = high_resolution_clock::now();
            frame_ipm = IPM(frame);
            auto ipm_end = high_resolution_clock::now();
            PERF_END("IPM_Transform");

            PERF_START("PIP_Overlay");
            auto pip_start = high_resolution_clock::now();
            // Apply picture-in-picture overlay
            frame = pictureInPicture(frame, frame_ipm);
            auto pip_end = high_resolution_clock::now();
            PERF_END("PIP_Overlay");
            
            // Calculate individual timing
            double ipm_time = duration_cast<microseconds>(ipm_end - ipm_start).count() / 1000.0;
            double pip_time = duration_cast<microseconds>(pip_end - pip_start).count() / 1000.0;
            
            // Display the frame
            imshow("Frame", frame);
            
            Mat output_frame;
            resize(frame, output_frame, Size(frame_width, frame_height));
            out.write(output_frame);

            // Calculate total frame processing time
            auto frame_end_time = high_resolution_clock::now();
            double total_frame_time = duration_cast<microseconds>(frame_end_time - frame_start_time).count() / 1000.0;

            // Update perf tracker
            perf_tracker.updateFrameStats(total_frame_time, ipm_time, pip_time);

            //Check for real-time perf
            double target_frame_time = 1000.0 / fps;
            if (total_frame_time > target_frame_time){
                LOG_WARNING("Frame " + to_string(frame_number) + " processing slow: " + 
                           to_string(total_frame_time) + "ms (target: " + to_string(target_frame_time) + "ms for " + 
                           to_string(fps) + " fps)");
            }

        } catch(const exception& e){
            LOG_ERROR("Error processing image " + image_path + ": " + e.what());
            continue; // Skip current image and continue
        }
        // Press 'q' to exit
        if (waitKey(1) == 'q') {
            LOG_INFO("Processing interrupted");
            break;
        }
    }
    // calculate total processing time
    auto total_end_time = high_resolution_clock::now();
    double total_processing_seconds = duration_cast<milliseconds>(total_end_time -total_start_time).count() / 1000.0;

    // Release video objects and close windows
    out.release();
    destroyAllWindows();

    // Log final performance summary
    LOG_INFO("=== Image Sequence Processing Completed ===");
    LOG_INFO("Total processing time: " + to_string(total_processing_seconds) + " seconds");
    LOG_INFO("Average processing speed: " + to_string(frame_number / total_processing_seconds) + " fps");
    LOG_INFO("Video saved as: " + output_video_path);
    
    perf_tracker.logSummary();
    
    return 0;
}   
int processVideo(const string& input_video_path, const string& output_video_path, int frame_width = 1280, int frame_height = 800){
    LOG_INFO("=== IPM Video Processing Started ===");

    // Performance Tracker
    PerformanceTracker perf_tracker;

    // Input and output file paths
    //string input_video_path = "../output_front.mp4";
    //string output_video_path = "carla_BEV_IPM_output_2.mp4";
    
    LOG_INFO("Input Video: " + input_video_path);
    LOG_INFO("Output Video: " + output_video_path);

    // Open the input video
    VideoCapture cap(input_video_path);
    
    // Check if video opened successfully
    if (!cap.isOpened()) {
        LOG_ERROR("Unable to open video file:" + input_video_path);
        delete g_logger; // why deleting?
        return -1;
    }
    
    // Get video properties
    double fps = cap.get(CAP_PROP_FPS);
    int total_frames = static_cast<int>(cap.get(CAP_PROP_FRAME_COUNT));
    LOG_INFO("Video properties: " + to_string(frame_width) + "x" + to_string(frame_height) + 
             " @ " + to_string(fps) + " fps, " + to_string(total_frames) + " frames");
    
    // Create VideoWriter object
    VideoWriter out(output_video_path, VideoWriter::fourcc('m', 'p', '4', 'v'), 
                    fps, Size(frame_width, frame_height));
    
    if (!out.isOpened()) {
        LOG_ERROR("Unable to create output video file: " + output_video_path);
        delete g_logger;
        return -1;
    }
    LOG_INFO("Video writer initialized successfully");

    Mat frame, frame_ipm;
    int frame_number = 0;
    auto total_start_time = high_resolution_clock::now();

    // Process the video
    while (true) {
        auto frame_start_time = high_resolution_clock::now();

        bool ret = cap.read(frame);
        if (!ret) {
            LOG_INFO("End of video reached. Processed " + to_string(frame_number) + " frames");
            break;
        }
        frame_number++;
        // Log Process every 100 frames
        if (frame_number % 100 == 0){
            LOG_INFO("Processing frame " + to_string(frame_number) + "/" + to_string(total_frames));
        }
        try{
            // Resize frame to desired dimensions
            resize(frame, frame, Size(frame_width, frame_height));
            
            // apply IPM transformation with timing
            PERF_START("IPM_Transform");
            auto ipm_start = high_resolution_clock::now();
            // Apply IPM transformation
            frame_ipm = IPM(frame);
            auto ipm_end = high_resolution_clock::now();
            PERF_END("IPM_Transform");

            PERF_START("PIP_Overlay");
            auto pip_start = high_resolution_clock::now();
            // Apply picture-in-picture overlay
            frame = pictureInPicture(frame, frame_ipm);
            auto pip_end = high_resolution_clock::now();
            PERF_END("PIP_Overlay");

            // Calculate individual timing
            double ipm_time = duration_cast<microseconds>(ipm_end - ipm_start).count() / 1000.0;
            double pip_time = duration_cast<microseconds>(pip_end - pip_start).count() / 1000.0;

            // For side-by-side instead of PIP, uncomment the following lines:
            // Mat frame_ipm_resized;
            // resize(frame_ipm, frame_ipm_resized, Size(frame_width/2, frame_height));
            // Mat combined;
            // hconcat(frame, frame_ipm_resized, combined);
            // frame = combined;
            
            // Display the frame
            imshow("Frame", frame);
            
            // Ensure frame is correct size before writing
            Mat output_frame;
            resize(frame, output_frame, Size(frame_width, frame_height));
            out.write(output_frame);

            // Calculate total frame processing time
            auto frame_end_time = high_resolution_clock::now();
            double total_frame_time = duration_cast<microseconds>(frame_end_time - frame_start_time).count() / 1000.0;

            // update performance tracker
            perf_tracker.updateFrameStats(total_frame_time, ipm_time, pip_time);

            //check for real-time per, targeting 30 fps target
            if (total_frame_time > 30){
                LOG_WARNING("Frame " + to_string(frame_number) + " processing slow: " + to_string(total_frame_time) + "ms (target 30 fps)");
            }
        } catch(const exception& e){
            LOG_ERROR("Error processing frame " + to_string(frame_number) +": " + e.what());
            continue; // skip curr frame and continue
        }
        
        // Press 'q' to exit the display window
        if (waitKey(1) == 'q') {
            break;
        }
    }
    // Calculate total processing time
    auto total_end_time = high_resolution_clock::now();
    double total_processing_seconds = duration_cast<milliseconds>(total_end_time - total_start_time).count() / 1000.0;

    // Release video objects and close windows
    cap.release();
    out.release();
    destroyAllWindows();
    
    // Log final perf summary
    LOG_INFO("=== Processing completed ===");
    LOG_INFO("Total processing time: " + to_string(total_processing_seconds) + " seconds");
    LOG_INFO("Average Processing speed: " + to_string(frame_number / total_processing_seconds) + " fps");
    LOG_INFO("Video saved as: " + output_video_path);

    perf_tracker.logSummary();
    return 0;
}
int main(int argc, char* argv[]) {
    // Initialize logger
    g_logger = new Logger("ipm_processing.log");
    
    // parge cmd line args
    if (argc < 2) {
        LOG_INFO("Usage:");
        LOG_INFO("  For video input: " + string(argv[0]) + " video <input_video_path> [output_video_path]");
        LOG_INFO("  For image sequence: " + string(argv[0]) + " images <input_directory> [output_video_path] [fps]");
        LOG_INFO("Examples:");
        LOG_INFO("  " + string(argv[0]) + " video ../output_front.mp4");
        LOG_INFO("  " + string(argv[0]) + " images ./waymo_images/ waymo_output.mp4 30");
        delete g_logger;
        return -1;
    }
    string mode = argv[1];
    int result = 0;

    if (mode == "video"){
        string input_video_path = (argc > 2) ? argv[2] : "../output_front.mp4";
        string output_video_path = (argc > 3) ? argv[3] : "carla_BEV_IPM_output_2.mp4";

        result = processVideo(input_video_path, output_video_path);
    } else if(mode == "images"){
        // image seq processing mode
        if (argc < 3) {
            LOG_ERROR("Image directory path required for images mode");
            delete g_logger;
            return -1;
        }
        string input_dir = argv[2];
        string output_video_path = (argc > 3) ? argv[3] : "waymo_BEV_IPM_output.mp4";
        double fps = (argc > 4) ? stod(argv[4]) : 30.0;

        result = processImageSequence(input_dir, output_video_path, fps);
    } else{
        LOG_ERROR("Invalid mode: " + mode + ". Use 'video' or 'images' ");
        result = -1;
    }
    //clean up
    delete g_logger;
    
    return 0;
}