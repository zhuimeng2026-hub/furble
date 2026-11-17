#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "FurbleControl.h"
#include "FurbleRemote.h"

namespace Furble {

static const char *REMOTE_TAG = "remote";

/* USB Serial/JTAG is registered as /dev/secondary when
 * CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG is enabled. The console only
 * writes to it; console_read() uses the primary (UART0) port, so the RX FIFO
 * is exclusively available for our command channel. */
static constexpr const char *REMOTE_PORT = "/dev/secondary";
static constexpr size_t REMOTE_LINE_MAX = 128;
static constexpr uint32_t REMOTE_TASK_STACK = 4096;
static constexpr uint8_t REMOTE_TASK_PRIORITY = 3;
static constexpr TickType_t REMOTE_POLL_MS = 10;

/* How long the AF-ON button is held down for a 'FOCUS' tap.
 * Needs to be long enough for the camera to acquire focus, a short press is
 * simply ignored by the camera. */
static constexpr uint32_t REMOTE_FOCUS_HOLD_MS = 400;

static int s_RemoteFd = -1;

void Remote::reply(const char *line) {
  if (s_RemoteFd < 0 || line == NULL) {
    return;
  }
  char buf[REMOTE_LINE_MAX + 4];
  int len = snprintf(buf, sizeof(buf), ">%s\r\n", line);
  if (len > 0) {
    (void)write(s_RemoteFd, buf, static_cast<size_t>(len));
  }
}

void Remote::handleLine(char *line) {
  /* Trim CR/LF and leading spaces. */
  while (*line == ' ' || *line == '\r' || *line == '\n') {
    line++;
  }
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\r' || line[len - 1] == '\n')) {
    line[--len] = '\0';
  }
  if (len == 0) {
    return;
  }

  auto &control = Control::getInstance();

  if (strcmp(line, "PING") == 0) {
    reply("PONG");
  } else if (strcmp(line, "SHUTTER") == 0) {
    control.sendCommand(Control::CMD_SHUTTER_PRESS);
    vTaskDelay(pdMS_TO_TICKS(100));
    control.sendCommand(Control::CMD_SHUTTER_RELEASE);
    reply("OK");
  } else if (strcmp(line, "SHUTTER_PRESS") == 0) {
    control.sendCommand(Control::CMD_SHUTTER_PRESS);
    reply("OK");
  } else if (strcmp(line, "SHUTTER_RELEASE") == 0) {
    control.sendCommand(Control::CMD_SHUTTER_RELEASE);
    reply("OK");
  } else if (strcmp(line, "FOCUS") == 0) {
    control.sendCommand(Control::CMD_FOCUS_PRESS);
    vTaskDelay(pdMS_TO_TICKS(REMOTE_FOCUS_HOLD_MS));
    control.sendCommand(Control::CMD_FOCUS_RELEASE);
    reply("OK");
  } else if (strcmp(line, "FOCUS_PRESS") == 0) {
    control.sendCommand(Control::CMD_FOCUS_PRESS);
    reply("OK");
  } else if (strcmp(line, "FOCUS_RELEASE") == 0) {
    control.sendCommand(Control::CMD_FOCUS_RELEASE);
    reply("OK");
  } else if (strcmp(line, "STATUS") == 0) {
    char buf[REMOTE_LINE_MAX];
    snprintf(buf, sizeof(buf), "STATE %d TARGETS %u",
             static_cast<int>(control.getState()),
             static_cast<unsigned int>(control.getTargets().size()));
    reply(buf);
    for (const auto &target : control.getTargets()) {
      auto *camera = target->getCamera();
      if (camera == nullptr) {
        continue;
      }
      char cbuf[REMOTE_LINE_MAX];
      snprintf(cbuf, sizeof(cbuf), "CAM %s %s", camera->getName().c_str(),
               camera->isConnected() ? "connected" : "disconnected");
      reply(cbuf);
    }
  } else if (strcmp(line, "HELP") == 0) {
    reply("CMDS PING SHUTTER SHUTTER_PRESS SHUTTER_RELEASE FOCUS FOCUS_PRESS FOCUS_RELEASE STATUS HELP");
  } else {
    reply("ERR unknown");
  }
}

void Remote::task(void *param) {
  (void)param;

  s_RemoteFd = open(REMOTE_PORT, O_RDWR);
  if (s_RemoteFd < 0) {
    ESP_LOGE(REMOTE_TAG, "failed to open %s", REMOTE_PORT);
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(REMOTE_TAG, "remote control ready on %s", REMOTE_PORT);

  char line[REMOTE_LINE_MAX];
  size_t used = 0;
  char chunk[REMOTE_LINE_MAX];

  while (true) {
    int rx = read(s_RemoteFd, chunk, sizeof(chunk) - 1);
    if (rx > 0) {
      for (int i = 0; i < rx; i++) {
        char c = chunk[i];
        if (c == '\n') {
          line[used] = '\0';
          handleLine(line);
          used = 0;
        } else if (c != '\r') {
          if (used < sizeof(line) - 1) {
            line[used++] = c;
          } else {
            /* Overflow: drop the partial line. */
            used = 0;
          }
        }
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(REMOTE_POLL_MS));
    }
  }
}

bool Remote::start(void) {
  BaseType_t ret = xTaskCreate(task, "remote", REMOTE_TASK_STACK, NULL,
                               REMOTE_TASK_PRIORITY, NULL);
  if (ret != pdPASS) {
    ESP_LOGE(REMOTE_TAG, "failed to create task");
    return false;
  }
  return true;
}

}  // namespace Furble
