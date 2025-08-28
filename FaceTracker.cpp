#include "FaceTracker.hpp"
#include <algorithm> // std::max_element
#include <iostream>

// Cascade file name (kept in same folder as executable / src)
static const std::string kCascadeFile = "resources/haarcascade_frontalface_default.xml";

bool FaceTracker::init(int cameraIndex) {
    // Load the face cascade
    if (!faceCascade.load(kCascadeFile)) {
        std::cerr << "Failed to load Haar cascade: " << kCascadeFile << '\n';
        return false;
    }

    // Open camera if requested
    if (cameraIndex >= 0) {
        capture.open(cameraIndex, cv::CAP_ANY);
        if (!capture.isOpened()) {
            std::cerr << "Failed to open camera\n";
            return false;
        }
    }
    return true;
}

std::optional<FaceResult> FaceTracker::grabAndDetect() {
    if (!capture.isOpened()) return std::nullopt;

    cv::Mat imgFrame;
    if (!capture.read(imgFrame) || imgFrame.empty()) {
        return std::nullopt; // no frame available
    }
    return detect(imgFrame);
}

FaceResult FaceTracker::detect(const cv::Mat& imgFrame) {
    FaceResult result;

    if (imgFrame.empty() || faceCascade.empty())
        return result;

    // Convert to grayscale and normalize contrast
    cv::Mat imgGray;
    cv::cvtColor(imgFrame, imgGray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(imgGray, imgGray);

    // Detect faces
    std::vector<cv::Rect> detectedFaces;
    faceCascade.detectMultiScale(imgGray,
                                 detectedFaces,
                                 1.1,          // scaleFactor
                                 3,            // minNeighbors
                                 0,            // flags
                                 cv::Size(40,40)); // minimum size

    if (detectedFaces.empty())
        return result;

    // Pick largest face (more stable for single-user scenarios)
    const cv::Rect face = getLargestFace(detectedFaces);

    // Compute centroid
    cv::Point2f centroidAbs(face.x + face.width  * 0.5f,
                            face.y + face.height * 0.5f);
    cv::Point2f centroidNorm(centroidAbs.x / imgFrame.cols,
                             centroidAbs.y / imgFrame.rows);

    result.faceFound         = true;
    result.centroidAbsolute  = centroidAbs;
    result.centroidNormalized= centroidNorm;

    return result;
}

cv::Rect FaceTracker::getLargestFace(const std::vector<cv::Rect>& faces) {
    return *std::max_element(faces.begin(), faces.end(),
                             [](const cv::Rect& a, const cv::Rect& b) {
                                 return a.area() < b.area();
                             });
}
