#ifndef FURBLE_CONTROL_H
#define FURBLE_CONTROL_H

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <Camera.h>

namespace Furble {

class Control {
 public:
  typedef enum {
    CMD_SHUTTER_PRESS,
    CMD_SHUTTER_RELEASE,
    CMD_FOCUS_PRESS,
    CMD_FOCUS_RELEASE,
    CMD_GPS_UPDATE,
    CMD_CONNECT,
    CMD_DISCONNECT,
    CMD_ERROR
  } cmd_t;

  typedef enum {
    /** No connections, waiting. */
    STATE_IDLE,
    /** Initiate connections. */
    STATE_CONNECT,
    /** Connections in progress. */
    STATE_CONNECTING,
    /** Initial connection attempt failed. */
    STATE_CONNECT_FAILED,
    /** All connections active. */
    STATE_ACTIVE,
    /** Disconnecting. */
    STATE_DISCONNECTING,
  } state_t;

  class Target {
    friend class Control;

   public:
    Target(Camera *camera);
    ~Target();

    Camera *getCamera(void) const;
    cmd_t getCommand(void);
    void sendCommand(cmd_t cmd);
    void updateGPS(const Camera::gps_t &gps, const Camera::timesync_t &timesync);

    void task(void);

   protected:
    volatile bool m_Stopped = false;

   private:
    static constexpr UBaseType_t m_QueueLength = 8;

    QueueHandle_t m_Queue = NULL;
    Furble::Camera *m_Camera = NULL;
    Camera::gps_t m_GPS;
    Camera::timesync_t m_Timesync;
  };

  static Control &getInstance();

  Control(Control const &) = delete;
  Control(Control &&) = delete;
  Control &operator=(Control const &) = delete;
  Control &operator=(Control &&) = delete;

  const uint32_t TIMEOUT_DEFAULT_MS = (30 * 1000);
  const uint32_t TIMEOUT_INFINITE_MS = (5 * 1000);
  const uint32_t SLEEP_INFINITE_MS = (5 * 1000);

  /**
   * FreeRTOS control task function.
   */
  void task(void);

  /**
   * Send control command to active connections.
   */
  BaseType_t sendCommand(cmd_t cmd);

  /**
   * Update GPS and timesync values.
   */
  BaseType_t updateGPS(const Camera::gps_t &gps, const Camera::timesync_t &timesync);

  /**
   * Are all active cameras still connected?
   */
  bool allConnected(void);

  /** Target status for a lock-safe snapshot. */
  struct TargetInfo {
    std::string name;
    bool connected;
  };

  /** Lock-safe snapshot of control state and target list (F-2). */
  struct Snapshot {
    state_t state;
    std::vector<TargetInfo> targets;
  };

  /** Result of submitIfActive(), so callers can report an accurate error. */
  typedef enum {
    SUBMIT_OK,             /**< Queued for dispatch. */
    SUBMIT_NOT_CONNECTED,  /**< No active camera connection. */
    SUBMIT_BUSY            /**< Camera connected but control queue is full. */
  } submit_result_t;

  /**
   * Enqueue a command only if a camera connection is active, and report
   * whether the command was actually accepted (F-1).
   */
  submit_result_t submitIfActive(cmd_t cmd);

  /** Lock-safe copy of state and targets, safe to iterate without the mutex. */
  Snapshot snapshot(void);

  /**
   * Get list of connected targets.
   *
   * @note Prefer snapshot() from tasks that cannot hold m_Mutex.
   */
  const std::vector<std::unique_ptr<Control::Target>> &getTargets(void);

  /**
   * Connect to all active cameras.
   */
  void connectAll(bool infiniteReconnect);

  /**
   * Disconnect all connected cameras.
   */
  void disconnect(void);

  /**
   * Add specified camera to active target list.
   */
  void addActive(Camera *camera);

  /**
   * Get current camera connection attempt.
   *
   * @return Camera being connected otherwise nullptr.
   */
  Camera *getConnectingCamera(void) const;

  /** Retrieve current control state. */
  state_t getState(void) const;

  /** Set transmit power. */
  void setPower(esp_power_level_t power);

 private:
  Control() {};

  /** Iterate over cameras and attempt connection. */
  state_t connectAll(void);

  static constexpr UBaseType_t m_QueueLength = 32;

  QueueHandle_t m_Queue = NULL;
  std::mutex m_Mutex;
  std::vector<std::unique_ptr<Control::Target>> m_Targets;

  bool m_InfiniteReconnect = false;
  state_t m_State = STATE_IDLE;

  // Camera connects are serialised, the following tracks the last attempt
  Camera *m_ConnectCamera = nullptr;
  esp_power_level_t m_Power = ESP_PWR_LVL_P3;
};

};  // namespace Furble

extern "C" {
void control_task(void *param);
}

struct FurbleCtx {
  Furble::Control *control;
  bool cancelled;
};

#endif
