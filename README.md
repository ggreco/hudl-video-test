# Node Video native module

This module allows a node or electron application to decode audio/video natively with frame accurate precision.

## Available APIs

### load

Signature: boolean load(string filename)

Load a local or remote (rtsp/http/https...) videofile. Most formats are supported.

### start

Signature: boolean start()

Start buffering video.

### frame

Signature: object frame([integer wait])

Get a YUV frame object from the video, a file must be loaded and the decoder should have been started.

object frame has the following properties:

width - frame width
height - frame height
pts - actual presentation timestamp, in seconds.
lum_data/u_data/v_data - node buffers containing the actual frame data (not copied, shared with the c++ core)

### rgb_frame

Signature: object rgb_frame([integer wait])

Get a RGB frame object from the video, a file must be loaded and the decoder should have been started.

object frame has the following properties:

width - frame width
height - frame height
pts - actual presentation timestamp, in seconds.
data - node buffers containing the actual frame data (not copied, shared with the c++ core) in BGR format (1 byte per component, no alpha)

### next

Signature: number next()

Get the pts of the next frame.

### eof

Signature: boolean eof()

Check if the file decoding is ended (and all the frames have been decoded)

### seek

Signature: boolean seek(number position)

Seek to a given PTS, in seconds.

### length

Signature: boolean length()

Get the total length of the loaded video (in seconds).

### get_direction/set_direction

Signature: string get_direction()
           boolean set_direction(string)

Get and the set the playback direction, you can set: forward/backward, values that can be returned from get_direction() are forward/backward/none.