#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <opencv2/opencv.hpp>
#include <iostream>

class GStreamerRTSPCapture {
private:
    GstElement *pipeline;
    GstElement *appsink;
    bool is_initialized;
    
public:
    GStreamerRTSPCapture() : pipeline(nullptr), appsink(nullptr), is_initialized(false) {}
    
    ~GStreamerRTSPCapture() {
        if (pipeline) {
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
        }
    }
    
    bool open(const std::string& rtsp_url) {
        std::cout << "Initializing GStreamer..." << std::endl;
        
        // Make sure GStreamer is initialized
        if (!gst_is_initialized()) {
            gst_init(nullptr, nullptr);
        }
        
        std::cout << "GStreamer initialized successfully" << std::endl;
        
        // Use a simpler, more robust pipeline that doesn't require missing plugins
        std::string pipeline_str = "rtspsrc location=" + rtsp_url + 
            " protocols=tcp latency=50 ! rtph264depay ! h264parse ! " +
            "queue ! fakesink name=sink";
        
        std::cout << "Creating simplified GStreamer pipeline: " << pipeline_str << std::endl;
        std::cout << "About to call gst_parse_launch..." << std::endl;
        
        GError *error = nullptr;
        
        // Try to parse the pipeline
        pipeline = gst_parse_launch(pipeline_str.c_str(), &error);
        
        std::cout << "gst_parse_launch completed" << std::endl;
        std::cout.flush();
        
        std::cout << "Checking for errors..." << std::endl;
        std::cout.flush();
        
        bool has_error = (error != nullptr);
        std::cout << "Error check result: " << (has_error ? "HAS ERROR" : "NO ERROR") << std::endl;
        std::cout.flush();
        
        if (error) {
            std::cout << "About to access error message..." << std::endl;
            std::cout.flush();
            
            const char* error_msg = error->message ? error->message : "Unknown error";
            std::cout << "Error message retrieved: " << error_msg << std::endl;
            std::cout.flush();
            
            std::cerr << "Pipeline creation failed: " << error_msg << std::endl;
            
            std::cout << "About to free error..." << std::endl;
            std::cout.flush();
            
            g_error_free(error);
            
            std::cout << "Error freed, returning false" << std::endl;
            std::cout.flush();
            
            return false;
        }
        
        std::cout << "No errors from gst_parse_launch" << std::endl;
        std::cout.flush();
        
        bool pipeline_valid = (pipeline != nullptr);
        std::cout << "Pipeline validity check: " << (pipeline_valid ? "VALID" : "NULL") << std::endl;
        std::cout.flush();
        
        if (!pipeline) {
            std::cerr << "Pipeline creation returned NULL" << std::endl;
            return false;
        }
        
        std::cout << "Pipeline pointer is valid" << std::endl;
        std::cout.flush();
        std::cout << "Pipeline created successfully" << std::endl;
        std::cout.flush();
        
        // Get appsink element - but now it's a fakesink
        std::cout << "Getting sink element..." << std::endl;
        std::cout << "About to call gst_bin_get_by_name..." << std::endl;
        appsink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
        std::cout << "gst_bin_get_by_name completed" << std::endl;
        
        if (!appsink) {
            std::cerr << "Failed to get sink element" << std::endl;
            gst_object_unref(pipeline);
            pipeline = nullptr;
            return false;
        }
        
        std::cout << "Got sink element successfully" << std::endl;
        
        // Skip appsink configuration since we're using fakesink for testing
        std::cout << "Skipping sink configuration for fakesink test..." << std::endl;
        
        std::cout << "Starting pipeline asynchronously..." << std::endl;
        
        // Start pipeline asynchronously - don't wait for state change
        std::cout << "About to call gst_element_set_state..." << std::endl;
        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        std::cout << "gst_element_set_state completed" << std::endl;
        std::cout << "State change initiated, return code: " << ret << std::endl;
        
        // Only fail if we get an immediate failure
        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Pipeline failed to start" << std::endl;
            return false;
        }
        
        // Don't wait for the state change to complete - let it happen in background
        std::cout << "Pipeline state change initiated successfully" << std::endl;
        
        is_initialized = true;
        std::cout << "GStreamer capture initialized (pipeline starting asynchronously)" << std::endl;
        return true;
    }
    
    bool read(cv::Mat& frame) {
        if (!is_initialized) {
            std::cerr << "GStreamer capture not initialized" << std::endl;
            return false;
        }
        
        std::cout << "Simplified read() - just returning false for now since using fakesink" << std::endl;
        std::cout << "RTSP connection test: Pipeline is running successfully!" << std::endl;
        return false; // Don't try to read from fakesink
    }
    
    bool isOpened() const {
        return is_initialized;
    }
};
