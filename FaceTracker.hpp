#pragma once
#include <opencv2/opencv.hpp>
#include <optional>
#include <string>

// Struct to hold detection results
struct FaceResult {
    bool faceFound = false;              // True if a face was detected
    cv::Point2f centroidAbsolute{0.f, 0.f}; // Center of face in pixels
    cv::Point2f centroidNormalized{0.f, 0.f}; // Center of face normalized to [0,1]
};

class FaceTracker {
public:
    FaceTracker() = default;
    ~FaceTracker() = default;

    // Initialize (cascade XML must be in the same folder as the exe/src)
    bool init(int cameraIndex = -1);

    // Grab a frame from the camera and run detection
    std::optional<FaceResult> grabAndDetect();

    // Run detection on an existing frame
    FaceResult detect(const cv::Mat& imgFrame);

    // Camera status helpers
    bool cameraOpened() const { return capture.isOpened(); }
    void release() { capture.release(); }

private:
    cv::VideoCapture capture;
    cv::CascadeClassifier faceCascade;

    static cv::Rect getLargestFace(const std::vector<cv::Rect>& faces);
};
