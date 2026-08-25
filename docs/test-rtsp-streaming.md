# BrightSign — Gaze Detection over RTSP (with MediaMTX)

This README gives you a practical, copy-pasteable guide to:

- what RTSP streaming is (and how our app uses it),
- how to publish a test RTSP stream with MediaMTX from a Linux box,
- how to run gaze detection on XT5 against that stream,
- how to tweak resolution/bitrate,
- notes for using a TP-Link Tapo C120 camera,
- how our RTSP/USB recovery mechanism works,
- and a short troubleshooting checklist.

## 1) RTSP in one minute

RTSP (Real Time Streaming Protocol) is a control protocol used to deliver live video over the network. A typical setup has:

- a **publisher/encoder** (camera or ffmpeg) that produces H.264/H.265 video,
- a **server** (e.g., MediaMTX) that relays it,
- one or more **clients** (our Gaze detection app), that receive, decode, and process frames.

In our pipeline on BrightSign device, we use GStreamer to pull NV12 frames from RTSP → convert/resize → run the RetinaFace model → publish results over UDP.

## 2) Quick start: publish a test stream with MediaMTX

### 2.1 Install MediaMTX on your Ubuntu machine

```bash
# Linux x64 example.
# Grab the latest linux_amd64 tarball from the releases page (asset names are
# versioned, e.g. mediamtx_v1.13.1_linux_amd64.tar.gz):
#   https://github.com/bluenviron/mediamtx/releases/latest
curl -L -o mediamtx.tgz "$(curl -s https://api.github.com/repos/bluenviron/mediamtx/releases/latest \
  | grep -oE 'https://github.com/bluenviron/mediamtx/releases/download/[^"]*linux_amd64\.tar\.gz' | head -1)"
tar xzf mediamtx.tgz
./mediamtx
```

MediaMTX will start with defaults and listen on:
- **RTSP**: `rtsp://<ubuntu-ip>:8554/`

(It also exposes RTMP, SRT, WebRTC by default—ignore for now.)

**Tip**: keep MediaMTX running in one terminal; use another terminal for ffmpeg/publisher.

### 2.2 Create a simple path in mediamtx.yml (optional but recommended)

Create a minimal config file `mediamtx.yml` alongside the binary:

```yaml
# mediamtx.yml
rtspAddress: :8554

paths:
  live:
    # The server will accept a publisher on this path.
    # No transcoding here—MediaMTX relays what you publish.
    # You'll control resolution/bitrate in the publisher (ffmpeg or the camera).
    source: publisher
```

Run MediaMTX with this config:

```bash
./mediamtx --config mediamtx.yml
```

Your stream URL will be `rtsp://<ubuntu-ip>:8554/live`.

### 2.3 Publish a test stream with ffmpeg (USB cam or a file)

**From a webcam** (e.g., `/dev/video0`) and re-encode to H.264 at 1280×720, 25 fps:

```bash
ffmpeg -f v4l2 -input_format mjpeg -video_size 1280x720 -framerate 25 -i /dev/video0 \
  -vf format=yuv420p,scale=1280:720 \
  -c:v libx264 -preset veryfast -tune zerolatency -profile:v baseline -b:v 2000k -maxrate 2000k -bufsize 1000k -g 50 \
  -f rtsp -rtsp_transport tcp rtsp://<ubuntu-ip>:8554/live
```

**From a file**:

```bash
ffmpeg -re -stream_loop -1 -i sample.mp4 \
  -vf format=yuv420p,scale=1280:720 \
  -c:v libx264 -preset veryfast -tune zerolatency -b:v 2000k -maxrate 2000k -bufsize 1000k -g 50 \
  -f rtsp -rtsp_transport tcp rtsp://<ubuntu-ip>:8554/live
```

The `-vf format=yuv420p,scale=WxH` plus `libx264` ensures a very compatible H.264 stream that our GStreamer pipeline likes.

## 3) Update the registry option to choose the video device on XT5/XS156 and reboot

```bash
registry write extension bsext-gaze-video-device rtsp://<ubuntu-ip>:8554/live
```

You should see logs like:

```
Detected RTSP URL: rtsp://192.168.0.203:8554/live
Trying pipeline 1/… 
RTSP NV12 appsink opened: 1280x720
Capture opened successfully
...
Performance: 25.0 FPS | Frame 320x320 | avg capture+convert: ~40 ms | avg inference: ~10 ms
```

### What makes it fast

- Hardware decode (`mppvideodec`),
- NV12 → NV12 resize first (320×320), then NV12 → RGB,
- RGA acceleration for color and resize,
- Producer/consumer design to overlap decode/convert with inference,
- Preallocated buffers, minimal allocations in the hot path.

## 4) Changing resolution/bitrate

**Key point**: MediaMTX does not transcode by default; it relays whatever the publisher sends. So, to change the resolution/bitrate:

1. **If you publish with ffmpeg**, change the scaling and rate control there:

```bash
# Example: 960x540 at ~1 Mbps
-vf format=yuv420p,scale=960:540 -b:v 1000k -maxrate 1000k -bufsize 500k
```

2. **If you're using an IP camera**, change its encoding settings (resolution, fps, codec) in the camera's web/app UI. The path in `mediamtx.yml` just forwards what the camera publishes.

3. **If you truly need server-side transcoding**, you can set up a separate ffmpeg process that pulls from one RTSP path and pushes a re-encoded feed into another MediaMTX path, e.g.:

```bash
# pull from live/raw → transcode → publish to live/processed
ffmpeg -rtsp_transport tcp -i rtsp://<ubuntu-ip>:8554/live \
  -vf format=yuv420p,scale=1280:720 \
  -c:v libx264 -preset veryfast -tune zerolatency -b:v 2000k -maxrate 2000k -bufsize 1000k \
  -f rtsp -rtsp_transport tcp rtsp://<ubuntu-ip>:8554/processed
```

Then point at `rtsp://<ubuntu-ip>:8554/processed`.

## 5) Using a Tapo C120 camera (notes)

- Ensure **RTSP/ONVIF streaming** is enabled in the Tapo app (exact menu names vary by firmware—look for "Advanced Settings", "Third-Party NVR", "RTSP/ONVIF", etc.).
- Create a **camera username/password** (Tapo often requires a dedicated "camera account").
- Typical URL patterns are like:

```
rtsp://<user>:<pass>@<camera-ip>:554/stream1   # main stream (higher res)
rtsp://<user>:<pass>@<camera-ip>:554/stream2   # sub stream (lower res)
```

- Choose the stream that matches your desired fps/bitrate/resolution (many cameras offer 25/30 fps in the main stream).
- You can point XT5/XS156 directly at the camera's RTSP URL (no MediaMTX needed), or have the camera publish into MediaMTX and point XT5 at MediaMTX.
- If colors look wrong or the stream won't open, try forcing H.264 in the camera and lower the resolution/fps briefly to test stability.

## 6) RTSP & USB recovery mechanism (how the app self-heals)

The inference worker:

- **Does not hard-fail** when the source disappears.
- It enters an **acquire → run → detect-stall → reacquire** loop:
  - **Acquire**: try the configured RTSP pipelines (TCP/UDP, H.264/H.265, decodebin fallback).
  - If RTSP fails, probe USB (V4L2).
  - Once open, the producer thread feeds frames to a 1-slot "latest frame" buffer.
  - If frames stall for ~3s (RTSP dropout / USB unplug), the producer flags the source as broken.
  - The consumer joins the producer thread, tears down, and re-runs acquire with exponential backoff (500→5000 ms).
- If there's no source at startup, the worker keeps retrying (it no longer exits immediately).
- If you send SIGTERM/SIGINT, it stops gracefully.

This allows:
- **RTSP camera reboot / Wi-Fi drops** → auto re-connect,
- **USB unplug/replug** → auto re-open (`/dev/videoX` re-probe).

## 7) Handy commands & quick checks

### Test with GStreamer (on your dev box)
```bash
gst-launch-1.0 -v rtspsrc location=rtsp://<ip>:8554/live protocols=tcp latency=150 ! \
  rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! autovideosink
```

### Check reachability from BrightSign device
```bash
ping <ubuntu-ip>
# If you use TCP:8554 ensure it's reachable
```

### If the stream doesn't open on BrightSign device

- Try H.264 instead of H.265.
- Lower the resolution/bitrate/fps in ffmpeg or camera settings.
- Ensure `GST_PLUGIN_PATH` and `GST_PLUGIN_SCANNER` are valid on XT5 (the app prints them on startup).
- Watch for logs like:
  - `"appsink opened: WxH"` → decoder OK
  - `"First-frame timeout"` → server path/caps issue or network path blocked
  - `"Could not open resource"` → RTSP URL/auth/connectivity issue

## 8) Known-good pipeline (the fast path we use)

When the upstream is H.264/H.265 and sane caps, we'll land on something equivalent to:

```
rtspsrc location=... protocols=tcp latency=150 drop-on-latency=true do-rtsp-keep-alive=true \
! application/x-rtp,media=video,encoding-name=H264|H265 \
! rtp(h264|h265)depay ! h264parse|h265parse \
! mppvideodec \
! queue max-size-buffers=2 leaky=downstream \
! appsink caps=video/x-raw,format=NV12 drop=1 max-buffers=1 enable-last-sample=false sync=false
```

Internally we:
- resize NV12 → 320×320,
- convert NV12 → RGB,
- run the model,
- print periodic perf lines like:

```
Performance: 25.0 FPS | Frame 320x320 | avg capture+convert: 40.0 ms | avg inference: 10.0 ms
```

## 9) Safety limits & perf notes

- **25–30 fps** at 320×320 is typical on XT5 with our optimized path.
- Keep GStreamer queue sizes tiny (we do) to avoid latency buildup.
- Prefer **TCP RTSP** when testing (more predictable through NAT/Wi-Fi).
- If you see color issues from some servers, we map NV12 via `GstVideoFrame` and convert precisely.

## 10) FAQ

**Q: MediaMTX isn't changing my resolution—why?**
A: It's a relay by default. Change the resolution at the publisher (ffmpeg or camera), or add a transcoding ffmpeg middlebox that repackages into another MediaMTX path.

**Q: My Tapo C120 URL doesn't work.**
A: Make sure RTSP/ONVIF is enabled in the app and a camera username/password is created. Try both stream1/stream2. Confirm with VLC on a laptop first.

**Q: What happens if I unplug the USB camera?**
A: The producer thread detects the missing `/dev/videoX`, marks the source as broken, and the worker re-acquires (it probes again until the device reappears).

## 11) Example end-to-end

**On Ubuntu:**

```bash
./mediamtx --config mediamtx.yml   # exposes rtsp://<ubuntu-ip>:8554/live
ffmpeg -re -stream_loop -1 -i sample.mp4 \
  -vf format=yuv420p,scale=1280:720 \
  -c:v libx264 -preset veryfast -tune zerolatency -b:v 2000k -maxrate 2000k -bufsize 1000k \
  -f rtsp -rtsp_transport tcp rtsp://<ubuntu-ip>:8554/live
```

**On XT5/XS156:**

Update the registry option for bsext-gaze-video-device to rtsp://<ubuntu-ip>:8554/live and reboot


Expect **~25–30 fps** with logs confirming NV12 appsink open and periodic performance lines.