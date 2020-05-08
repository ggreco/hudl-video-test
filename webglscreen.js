class WebglScreen {
    /**
     * Create a new Webgl context for video output.
     * 
     * The context may use BGR24 or YUV (12bpp) source images.
     * 
     * @param {Object} canvas DOM canvas object to be used as target 
     * @param {boolean} rgb If we are using bgr images as source, defaults to false (YUV)
     */
    constructor(canvas, rgb) {
        this.canvas = canvas;
        this.gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');
        this.rgb = (rgb === true)
        this._init();
    }

    _init() {
        let gl = this.gl;
        if (!gl) {
            console.log('gl not support!');
            return;
        }
        // Image preprocessing
        gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
        // Vertex shader code in GLSL format

        let vertexShaderSource = `
            attribute lowp vec4 a_vertexPosition;
            attribute vec2 a_texturePosition;
            varying vec2 v_texCoord;
            void main() {
                gl_Position = a_vertexPosition;
                v_texCoord = a_texturePosition;
            }
        `;

        let fragmentShaderSource = this.rgb ?
        `
            precision lowp float;
            uniform sampler2D samplerY;
            varying vec2 v_texCoord;
            void main() {
                float r,g,b;
                r = texture2D(samplerY, v_texCoord).r;
                g = texture2D(samplerY, v_texCoord).g;
                b = texture2D(samplerY, v_texCoord).b;
                gl_FragColor = vec4(b, g, r, 1.0);
            }
        ` :
        `
            precision lowp float;
            uniform sampler2D samplerY;
            uniform sampler2D samplerU;
            uniform sampler2D samplerV;
            varying vec2 v_texCoord;
            void main() {
                float r,g,b,y,u,v,fYmul;
                y = texture2D(samplerY, v_texCoord).r;
                u = texture2D(samplerU, v_texCoord).r;
                v = texture2D(samplerV, v_texCoord).r;

                fYmul = y * 1.1643828125;
                r = fYmul + 1.59602734375 * v - 0.870787598;
                g = fYmul - 0.39176171875 * u - 0.81296875 * v + 0.52959375;
                b = fYmul + 2.01723046875 * u - 1.081389160375;
                gl_FragColor = vec4(r, g, b, 1.0);
            }
        `;

        let vertexShader = this._compileShader(vertexShaderSource, gl.VERTEX_SHADER);
        let fragmentShader = this._compileShader(fragmentShaderSource, gl.FRAGMENT_SHADER);

        let program = this._createProgram(vertexShader, fragmentShader);

        this._initVertexBuffers(program);
                
        // Activates the specified texture unit
        gl.activeTexture(gl.TEXTURE0);
        gl.y = this._createTexture();
        gl.uniform1i(gl.getUniformLocation(program, 'samplerY'), 0);

        if (!this.rgb) {
            gl.activeTexture(gl.TEXTURE1);
            gl.u = this._createTexture();
            gl.uniform1i(gl.getUniformLocation(program, 'samplerU'), 1);

            gl.activeTexture(gl.TEXTURE2);
            gl.v = this._createTexture();
            gl.uniform1i(gl.getUniformLocation(program, 'samplerV'), 2);
        }
    }
    /**
     * Initialize vertex buffer
     * @param {glProgram} program program
     */

    _initVertexBuffers(program) {
        let gl = this.gl;
        let vertexBuffer = gl.createBuffer();
        let vertexRectangle = new Float32Array([
            1.0,
            1.0,
            0.0,
            -1.0,
            1.0,
            0.0,
            1.0,
            -1.0,
            0.0,
            -1.0,
            -1.0,
            0.0
        ]);
        gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
        // Write data to buffer
        gl.bufferData(gl.ARRAY_BUFFER, vertexRectangle, gl.STATIC_DRAW);
        // Location of vertices found
        let vertexPositionAttribute = gl.getAttribLocation(program, 'a_vertexPosition');
        // Tell the graphics card to read vertex data from the currently bound buffer
        gl.vertexAttribPointer(vertexPositionAttribute, 3, gl.FLOAT, false, 0, 0);
        // Connect the vertexPosition variable to the buffer object assigned to it
        gl.enableVertexAttribArray(vertexPositionAttribute);

        let textureRectangle = new Float32Array([1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0]);
        let textureBuffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, textureBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, textureRectangle, gl.STATIC_DRAW);
        let textureCoord = gl.getAttribLocation(program, 'a_texturePosition');
        gl.vertexAttribPointer(textureCoord, 2, gl.FLOAT, false, 0, 0);
        gl.enableVertexAttribArray(textureCoord);
    }

    /**
     * Create and compile a shader
     * @param {string} shaderSource GLSL Shader code for format
     * @param {number} shaderType Shader type, VERTEX_SHADER or FRAGMENT_SHADER.
     * @return {glShader} Shader.
     */
    _compileShader(shaderSource, shaderType) {
        // Create Shader Program
        let shader = this.gl.createShader(shaderType);
        // Set source code for shader
        this.gl.shaderSource(shader, shaderSource);
        // Compile Shader
        this.gl.compileShader(shader);
        const success = this.gl.getShaderParameter(shader, this.gl.COMPILE_STATUS);
        if (!success) {
            let err = this.gl.getShaderInfoLog(shader);
            this.gl.deleteShader(shader);
            console.error('could not compile shader', err);
            return;
        }

        return shader;
    }

    /**
     * Create a program from two shaders
     * @param {glShader} vertexShader Vertex shader.
     * @param {glShader} fragmentShader Fragment shader.
     * @return {glProgram} program
     */
    _createProgram(vertexShader, fragmentShader) {
        const gl = this.gl;
        let program = gl.createProgram();

        // Attach shader
        gl.attachShader(program, vertexShader);
        gl.attachShader(program, fragmentShader);

        gl.linkProgram(program);
        // Add WebGLProgram object to current rendering state
        gl.useProgram(program);
        const success = this.gl.getProgramParameter(program, this.gl.LINK_STATUS);

        if (!success) {
            console.err('program fail to link' + this.gl.getShaderInfoLog(program));
            return;
        }

        return program;
    }

    /**
     * Set Texture
     */
    _createTexture(filter = this.gl.LINEAR) {
        let gl = this.gl;
        let t = gl.createTexture();
        // Bind the given glTexture to the target (binding point)
        gl.bindTexture(gl.TEXTURE_2D, t);
        // Texture packaging refers to https://github.com/fem-d/webGL/blob/master/blog/WebGL Basic Learning Paper (Lesson%207). MD -> Texture wrapping
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        // Set Texture Filtering
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter);
        return t;
    }

    /**
     * Render the picture
     * 
     * Only dataY is needed if RGB.
     * 
     * @param {number} width width
     * @param {number} height height
     * @param {Uint8Array} dataY luminance component (if YUV) or BGR buffer
     * @param {Uint8Array} dataU chroma component U (only for YUV surfaces), it's size should be 1/4 of luminance
     * @param {Uint8Array} dataV chroma component V (only for YUV surfaces), it's size should be 1/4 of luminance
     */
    renderImage(width, height, dataY, dataU, dataV) {
        let gl = this.gl;
        // Set the viewport, that is, specify the x, y affine transformation from the standard device to the window coordinates
        gl.viewport(0, 0, width, height);
        // Set the color value when emptying the color buffer
        gl.clearColor(0, 0, 0, 0);
        // Empty Buffer
        gl.clear(gl.COLOR_BUFFER_BIT);

        gl.bindTexture(gl.TEXTURE_2D, gl.y);
        // Fill Texture
        if (this.rgb) {
            gl.texImage2D(
                gl.TEXTURE_2D,
                0,
                gl.RGB,
                width,
                height,
                0,
                gl.RGB,
                gl.UNSIGNED_BYTE,
                new Uint8Array(dataY)
            );
        } else {
            gl.texImage2D(
                gl.TEXTURE_2D,
                0,
                gl.LUMINANCE,
                width,
                height,
                0,
                gl.LUMINANCE,
                gl.UNSIGNED_BYTE,
                new Uint8Array(dataY)
            );

            gl.bindTexture(gl.TEXTURE_2D, gl.u);
            gl.texImage2D(
                gl.TEXTURE_2D,
                0,
                gl.LUMINANCE,
                width >> 1,
                height >> 1,
                0,
                gl.LUMINANCE,
                gl.UNSIGNED_BYTE,
                new Uint8Array(dataU)
            );

            gl.bindTexture(gl.TEXTURE_2D, gl.v);
            gl.texImage2D(
                gl.TEXTURE_2D,
                0,
                gl.LUMINANCE,
                width >> 1,
                height >> 1,
                0,
                gl.LUMINANCE,
                gl.UNSIGNED_BYTE,
                new Uint8Array(dataV)
            );
        }
        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
    }

    /**
     * Based on resetting canvas size
     * @param {number} width width
     * @param {number} height height
     */
    setSize(width, height) {
        this.canvas.width = width;
        this.canvas.height = height;
    }

    destroy() {
        const {
            gl
        } = this;

        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT | gl.STENCIL_BUFFER_BIT);
    }
}

module.exports = WebglScreen;