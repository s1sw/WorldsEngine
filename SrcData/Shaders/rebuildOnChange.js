// yes it's javascript
// whatcha gonna do about it

const fs = require("fs");
const child_process = require("child_process");

const buildDebugShaders = false;

const customArgs = {
    "standard.glsl": [
        { stage: "frag", defines: ["FRAGMENT", "EFT"], outFile: "standard.frag.spv" },
        { stage: "frag", defines: ["FRAGMENT"], outFile: "standard_alpha_test.frag.spv" },
        { stage: "vert", defines: ["VERTEX"], outFile: "standard.vert.spv" },
    ],
    "tonemap.comp.glsl": [
        { stage: "comp", defines: ["MSAA"], outFile: "tonemap.comp.spv" },
        { stage: "comp", defines: [], outFile: "tonemap_nomsaa.comp.spv" }
    ],
    "ui.glsl": [
        { stage: "frag", defines: ["FRAGMENT"], outFile: "ui.frag.spv" },
        { stage: "vert", defines: ["VERTEX"], outFile: "ui.vert.spv" }
    ]
};

function findSourceFiles() {
    let files = fs.readdirSync(".");
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
    fs.rename(filename, `../../Data/Shaders/${filename}`, callback);
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

const files = findSourceFiles();

for (let f of files) {
    fs.watchFile(f, {"persistent": true, "interval":1000}, (_, __) => {
        console.log(`File ${f} changed`);
        buildFile(f);
    });
}

fs.watchFile("Include", {"interval":1000}, (_, __) => {
    rebuildAll();
});

rebuildAll();
console.log(`Watching ${files.length} files`);
