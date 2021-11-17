// yes it's javascript
// whatcha gonna do about it

const fs = require("fs");
const child_process = require("child_process");

const buildDebugShaders = process.argv.includes("--debug");

if (buildDebugShaders) {
    console.log("--- BUILDING DEBUG SHADERS ---");
}

const customArgs = {
    "standard.glsl": [
        { stage: "frag", defines: ["FRAGMENT", "EFT"], outFile: "standard.frag.spv" },
        { stage: "frag", defines: ["FRAGMENT"], outFile: "standard_alpha_test.frag.spv" },
        { stage: "vert", defines: ["VERTEX"], outFile: "standard.vert.spv" },
        { stage: "vert", defines: ["VERTEX", "SKINNED"], outFile: "standard_skinned.vert.spv" }
    ],
    "tonemap.comp.glsl": [
        { stage: "comp", defines: ["MSAA"], outFile: "tonemap.comp.spv" },
        { stage: "comp", defines: [], outFile: "tonemap_nomsaa.comp.spv" }
    ],
    "light_cull.comp.glsl": [
        { stage: "comp", defines: ["MSAA"], outFile: "light_cull.comp.spv" },
        { stage: "comp", defines: [], outFile: "light_cull_nomsaa.comp.spv" }
    ],
    "ui.glsl": [
        { stage: "frag", defines: ["FRAGMENT"], outFile: "ui.frag.spv" },
        { stage: "vert", defines: ["VERTEX"], outFile: "ui.vert.spv" }
    ],
    "depth_prepass.vert.glsl": [
        { stage: "vert", defines: [], outFile: "depth_prepass.vert.spv" },
        { stage: "vert", defines: ["SKINNED"], outFile: "depth_prepass_skinned.vert.spv" }
    ],
    "bloom_blur.comp.glsl": [
        { stage: "comp", defines: ["SEED"], outFile: "bloom_blur_seed.comp.spv" },
        { stage: "comp", defines: [], outFile: "bloom_blur.comp.spv" }
    ],
};

function findSourceFiles(dir) {
    let files = fs.readdirSync(dir);
    return files.filter(file => file.match(new RegExp(`.*\.(.glsl)`, 'ig')));
}

function getDefaultArgs() {
    return ["--target-env=vulkan1.2", "-I", "Include"];
}

function getDefineArgs(defines) {
    let args = defines.map((d) => {
        return "-D" + d;
    });

    if (buildDebugShaders) {
        args.push("-DDEBUG");
    }

    return args;
}

function getArgList(stage, defines, inFile, outFile) {
    return getDefaultArgs().concat(
        getDefineArgs(defines).concat(
            [
                `-fshader-stage=${stage}`,
                `${inFile}`,
                `-o`,
                `${outFile}`
            ])
    );
}

function moveToOutput(filename, callback) {
    fs.rename(filename, `../../EngineData/Shaders/${filename}`, callback);
}

function execCompiler(args, onSuccess) {
    child_process.execFile("glslc", args, (err, stdout, stderr) => {
        if (stdout.length > 0)
            console.log(stdout.trim());

        if (stderr.length > 0)
            console.log(stderr.trim());

        if (!err)
            onSuccess();
    });
}

function buildFile(filename) {
    if (!customArgs.hasOwnProperty(filename)) {
        let splitFilename = filename.split(".");

        splitFilename[splitFilename.length - 1] = "spv";
        let outputFilename = splitFilename.join(".");
        let shaderStage = splitFilename[splitFilename.length - 2];

        let args = getArgList(shaderStage, [], filename, outputFilename);

        execCompiler(args, () => {
            moveToOutput(outputFilename, () => {
                console.log(`Compiled ${filename}`);
            });
        });

    } else {
        let fileArgs = customArgs[filename];
        let counter = 1;

        for (let arg of fileArgs) {
            let args = getArgList(arg.stage, arg.defines, filename, arg.outFile);

            execCompiler(args, () => {
                moveToOutput(arg.outFile, () => {
                    console.log(`Compiled ${filename} (${counter}/${fileArgs.length})`);
                    counter++;
                });
            });
        }
    }
}

function rebuildAll() {
    for (let file of files) {
        buildFile(file);
    }
}

const files = findSourceFiles(".");

for (let f of files) {
    fs.watchFile(f, {"persistent": true, "interval":1000}, (_, __) => {
        console.log(`File ${f} changed`);
        buildFile(f);
    });
}

for (let f of findSourceFiles("Include")) {
    fs.watchFile(`Include/${f}`, {"interval":1000}, (_, __) => {
        rebuildAll();
    });
}

rebuildAll();
console.log(`Watching ${files.length} files`);
