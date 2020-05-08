const wyDecoder = require('./build/Release/wydecoder.node');
const args = process.argv.slice(2);

console.log(" exports: ", wyDecoder);

wyDecoder.load(args[0]);
wyDecoder.start();

var rgb = false;
while (!wyDecoder.eof()) {
    const obj = rgb ? wyDecoder.rgb_frame(10) : wyDecoder.frame(10);

    if (obj.pts) {
        if (rgb)
            console.log("RGB Frame: " + obj.width + "x" + obj.height + " pts: " + obj.pts + 
                " data:" + obj.data.byteLength 
            );
        else
            console.log("YUV Frame: " + obj.width + "x" + obj.height + " pts: " + obj.pts + 
                " dataY:" + obj.lum_data.byteLength +
                " dataU:" + obj.u_data.byteLength +
                " dataV:" + obj.v_data.byteLength
            );
        // const buf = new Uint8Array(obj.data);
        // console.log("Val: " + buf[256] + " " + buf[512] + " "  + buf[1024]);
        rgb = rgb ? false : true;
    } else
        console.log("....waiting...");
}