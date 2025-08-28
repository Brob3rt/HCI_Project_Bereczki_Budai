#include "FaceTracker.hpp"
#include <algorithm>   // std::max_element
#include <iostream>    // std::cerr

// Haar cascade XML should be next to your sources/executable.
static const char* kCascadeFilename = "resources/haarcascade_frontalface_default.xml";


FaceTracker::~FaceTracker() {
    // Ensure clean shutdown if the user forgot to stop the worker
    stopWorker();
}

bool FaceTracker::init(int camera_index) {
    // 1) Load detector
    if (!faceCascade_.load(kCascadeFilename)) {
        std::cerr << "Failed to load cascade: " << kCascadeFilename << '\n';
        return false;
    }
    // 2) Open camera
    if (!capture_.open(camera_index, cv::CAP_ANY)) {
        std::cerr << "Failed to open camera index " << camera_index << '\n';
        return false;
    }
    return true;
}

// ------------------------
// Single-thread (pull) API
// ------------------------
std::optional<FaceResult> FaceTracker::grabAndDetect() {
    if (!capture_.isOpened()) return std::nullopt;

    cv::Mat frame;
    if (!capture_.read(frame) || frame.empty()) {
        return std::nullopt; // camera closed or frame read failed
    }

    return detect(frame);
}

FaceResult FaceTracker::detect(const cv::Mat& frame) {
    FaceResult result;
    if (frame.empty() || faceCascade_.empty()) {
        return result; // face_found remains false
    }

    cv::Point2f center_px, center_norm;
    if (detectFaceCenter(frame, center_px, center_norm)) {
        result.face_found  = true;
        result.center_px   = center_px;
        result.center_norm = center_norm;
    }
    return result;
}

// ------------------------
// Multithreaded (worker) API
// ------------------------
bool FaceTracker::startWorker() {
    if (!capture_.isOpened() || workerIsRunning_.load(std::memory_order_relaxed)) {
        return false;
    }
    stopRequested_.store(false, std::memory_order_relaxed);
    endOfStream_.store(false, std::memory_order_relaxed);
    workerIsRunning_.store(true, std::memory_order_relaxed);

    workerThread_ = std::thread(&FaceTracker::trackerThreadLoop, this);
    return true;
}

void FaceTracker::stopWorker() {
    if (!workerIsRunning_.load(std::memory_order_relaxed)) return;
    stopRequested_.store(true, std::memory_order_relaxed);
    if (workerThread_.joinable()) workerThread_.join();
    workerIsRunning_.store(false, std::memory_order_relaxed);
}

std::optional<FaceResult> FaceTracker::getLatest(std::uint64_t& last_seq) const {
    const auto current_seq = resultSequence_.load(std::memory_order_relaxed);
    if (current_seq == last_seq) {
        return std::nullopt; // no new result since last poll
    }
    last_seq = current_seq;

    FaceResult r;
    r.face_found = lastFaceFound_.load(std::memory_order_relaxed);
    if (r.face_found) {
        r.center_norm = {
            lastCenterNormX_.load(std::memory_order_relaxed),
            lastCenterNormY_.load(std::memory_order_relaxed)
        };
        r.center_px = {
            lastCenterPixX_.load(std::memory_order_relaxed),
            lastCenterPixY_.load(std::memory_order_relaxed)
        };
    }
    return r;
}

// ------------------------
// Worker internals
// ------------------------
void FaceTracker::trackerThreadLoop() {
    while (!stopRequested_.load(std::memory_order_relaxed)) {
        cv::Mat frame;
        if (!capture_.read(frame) || frame.empty()) {
            endOfStream_.store(true, std::memory_order_relaxed);
            break;
        }

        cv::Point2f center_px, center_norm;
        const bool found = detectFaceCenter(frame, center_px, center_norm);

        // Publish the latest result atomically
        lastFaceFound_.store(found, std::memory_order_relaxed);
        if (found) {
            lastCenterPixX_.store(center_px.x, std::memory_order_relaxed);
            lastCenterPixY_.store(center_px.y, std::memory_order_relaxed);
            lastCenterNormX_.store(center_norm.x, std::memory_order_relaxed);
            lastCenterNormY_.store(center_norm.y, std::memory_order_relaxed);
        }
        resultSequence_.fetch_add(1, std::memory_order_relaxed);

        // Optional throttling:
        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ------------------------
// Helpers
// ------------------------
bool FaceTracker::detectFaceCenter(const cv::Mat& frame,
                                   cv::Point2f& center_px,
                                   cv::Point2f& center_norm) {
    // Preprocess for robust detection
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    // Detect faces (tune params for speed/sensitivity as needed)
    std::vector<cv::Rect> faces;
    faceCascade_.detectMultiScale(
        gray,
        faces,
        1.1,            // scaleFactor
        3,              // minNeighbors
        0,              // flags
        cv::Size(40,40) // minSize to avoid tiny false positives
    );
    if (faces.empty()) return false;

    const cv::Rect face = largestFace(faces);

    // Compute pixel center & normalized center
    center_px = {
        face.x + face.width  * 0.5f,
        face.y + face.height * 0.5f
    };
    center_norm = {
        center_px.x / static_cast<float>(frame.cols),
        center_px.y / static_cast<float>(frame.rows)
    };
    return true;
}

cv::Rect FaceTracker::largestFace(const std::vector<cv::Rect>& faces) {
    return *std::max_element(
        faces.begin(), faces.end(),
        [](const cv::Rect& a, const cv::Rect& b) { return a.area() < b.area(); }
    );
}
