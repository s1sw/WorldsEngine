// yes it's javascript
// whatcha gonna do about it

const fs = require('fs');
const child_process = require('child_process');
const glob = require('glob');

const files = glob("*.glsl");

fs.watch(files, {"persistent": true}, (type, filename) => {
    child_process.exec('buildshaders.bat', (error, stdout, stderr) => {
        console.log(stdout);
    });
});
