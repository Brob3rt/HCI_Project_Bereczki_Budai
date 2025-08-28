#pragma once

#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>

// A single detection result (for pull or push-style access)
struct FaceResult {
    bool        face_found = false;             // Was a face detected?
    cv::Point2f center_px{0.f, 0.f};            // Center in pixels
    cv::Point2f center_norm{0.f, 0.f};          // Center normalized to [0,1]
};

class FaceTracker {
public:
    FaceTracker() = default;
    ~FaceTracker();

    // Initialize: load Haar cascade (from same folder) and open the camera.
    bool init(int camera_index);

    // --- Single-thread API (compatible with your existing main.cpp) ---
    // Capture a frame and run detection (returns std::nullopt if grabbing fails).
    std::optional<FaceResult> grabAndDetect();

    // Run detection on a provided frame (no grabbing).
    FaceResult detect(const cv::Mat& frame);

    // --- Multithreaded API (worker thread that grabs + detects continuously) ---
    bool startWorker();           // Launch worker thread
    void stopWorker();            // Request stop and join thread

    // Poll for the latest result only if a NEW one is available since 'last_seq'.
    // Usage:
    //   uint64_t last_seq = 0;
    //   if (auto res = tracker.getLatest(last_seq)) { ...use res... }
    std::optional<FaceResult> getLatest(std::uint64_t& last_seq) const;

    // Helpers
    bool cameraOpened() const { return capture_.isOpened(); }
    bool workerRunning() const { return workerIsRunning_.load(std::memory_order_relaxed); }
    void release() { capture_.release(); }

private:
    // ---- Capture & Detector ----
    cv::VideoCapture      capture_;
    cv::CascadeClassifier faceCascade_;

    // ---- Worker Thread State ----
    std::thread       workerThread_;
    std::atomic<bool> stopRequested_{false};   // main -> worker: please stop
    std::atomic<bool> endOfStream_{false};     // worker -> main: camera/file ended
    std::atomic<bool> workerIsRunning_{false}; // worker is currently running

    // ---- Published Result (lock-free reads via atomics) ----
    std::atomic<bool>  lastFaceFound_{false};
    std::atomic<float> lastCenterNormX_{0.f};
    std::atomic<float> lastCenterNormY_{0.f};
    std::atomic<float> lastCenterPixX_{0.f};
    std::atomic<float> lastCenterPixY_{0.f};
    std::atomic<std::uint64_t> resultSequence_{0}; // incremented on each publish

    // Worker loop that grabs frames and publishes results.
    void trackerThreadLoop();

    // Helpers
    static cv::Rect largestFace(const std::vector<cv::Rect>& faces);

    // Compute center of largest face. Returns true if a face is found.
    bool detectFaceCenter(const cv::Mat& frame,
                          cv::Point2f& center_px,
                          cv::Point2f& center_norm);
};
