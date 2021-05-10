// yes it's javascript
// whatcha gonna do about it

const fs = require('fs');
const child_process = require('child_process');

const customArgs = {
    "standard.glsl": [
        { stage: "frag", defines: ["FRAGMENT", "EFT"], outFile: "standard.frag.spv" },
        { stage: "frag", defines: ["FRAGMENT"], outFile: "standard_alpha_test.frag.spv" },
        { stage: "vert", defines: ["VERTEX"], outFile: "standard.vert.spv" },
    ],
    "tonemap.comp.glsl": [
        { stage: "comp", defines: ["MSAA"], outFile: "tonemap.comp.spv" },
        { stage: "comp", defines: [], outFile: "tonemap_nomsaa.comp.spv" }
    ]
};

function findSourceFiles() {
    let files = fs.readdirSync(".");
    return files.filter(file => file.match(new RegExp(`.*\.(.glsl)`, 'ig')));
}

function moveCompiledFiles() {
    let files = fs.readdirSync(".");
    let spvFiles = files.filter(file => file.match(new RegExp(`.*\.(.spv)`, 'ig')));

    for (let file of spvFiles) {
        fs.rename(file, `../../Data/Shaders/${file}`, () => {});
    }

    if (spvFiles.length > 0)
        console.log(`Moved ${spvFiles.length} files`);
}

function getDefaultArgs() {
    return "--target-env=vulkan1.2 -I Include";
}

function getDefineArgs(defines) {
    if (defines.length == 0) return "";
    return "-D" + defines.join(" -D");
}

function getCmdString(stage, defines, inFile, outFile) {
    return `glslc ${getDefaultArgs()} -fshader-stage=${stage} ${getDefineArgs(defines)} ${inFile} -o ${outFile}`;
}

function onFileChange(curr, prev, filename) {
    console.log(`File ${filename} changed`);
    if (!customArgs.hasOwnProperty(filename)) {
        let splitFilename = filename.split(".");

        splitFilename[splitFilename.length - 1] = "spv";
        let outputFilename = splitFilename.join(".");
        let shaderStage = splitFilename[splitFilename.length - 2];

        let cmdString = getCmdString(shaderStage, [], filename, outputFilename);

        child_process.exec(cmdString, (error, stdout, stderr) => {
            if (stdout.length > 0)
                console.log(stdout.trim());
            fs.rename(outputFilename, `../../Data/Shaders/${outputFilename}`, ()=>{});
        });

        console.log(`Compiled ${filename}`);
    } else {
        let fileArgs = customArgs[filename];
        let idx = 1;

        for (let arg of fileArgs) {
            let cmdString = getCmdString(arg.stage, arg.defines, filename, arg.outFile);

            child_process.exec(cmdString, (error, stdout, stderr) => {
                if (stdout.length > 0)
                    console.log(stdout.trim());
                fs.rename(arg.outFile, `../../Data/Shaders/${arg.outFile}`, ()=>{});
            });

            console.log(`Compiled ${filename} (${idx}/${fileArgs.length})`);
            idx += 1;
        }

        console.log("Compilation complete");
    }

}

const files = findSourceFiles();

for (let f of files) {
    fs.watchFile(f, {"persistent": true, "interval":1000}, (curr, prev) => {
        onFileChange(curr, prev, f);
    });
}

console.log(`Watching ${files.length} files`);
