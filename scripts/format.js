const { promisify } = require('util');
const child_process = require('child_process');
const fs = require('fs');

const access = promisify(fs.access);
const readdir = promisify(fs.readdir);
const stat = promisify(fs.stat);
const exec = promisify(child_process.exec);

/* The directories to run our formatting on, recursively */
const directoriesToFormat = ['include', 'src'];

/* Filetypes to run the formatter on */
const fileTypes = ['.h', '.cpp', '.c'];

/* The name of the clang-format binary. In some distros this has the version
   appended. */
const binaryName = 'clang-format';

(async () => {
    const success = await main();

    if (success) {
        console.log('\nFormatting complete.');
    } else {
        console.log('\nFormatting aborted.');
    }
})();

async function main() {
    /* Check we're in the write folder and the directories exist */
    for (const directory of directoriesToFormat) {
        try {
            await access(directory);
        } catch (err) {
            console.log(`Failed to find ${directory} directory, probably in the wrong folder.`);
            console.log('Make sure to run from the root folder, like so: node scripts/format.js');
            return false;
        }
    }

    let filePaths = [];

    /* Gather the files */
    for (const directory of directoriesToFormat) {
        const files = await getFiles(directory);
        filePaths = filePaths.concat(files);
    }

    /* Filter out non code files */
    filePaths = filePaths.filter((file) => {
        return fileTypes.some((extension) => file.endsWith(extension));
    });

    console.log(`Found ${filePaths.length} files to format!`);

    /* Format each file */
    for (const file of filePaths) {
        await formatFile(file);
    }

    return true;
}

async function getFiles(directory) {
    const directoryContents = await readdir(directory);

    let allFiles = [];

    for (const file of directoryContents) {
        const fullPath = directory + '/' + file;

        const fileStats = await stat(fullPath);

        /* This is a directory, lets recurse */
        if (fileStats.isDirectory()) {
            const moreFiles = await getFiles(fullPath);
            allFiles = allFiles.concat(moreFiles);
        } else {
            allFiles.push(fullPath);
        }
    }

    return allFiles;
}

async function formatFile(filePath) {
    try {
        console.log(`Formatting ${filePath}`);

        await exec(`${binaryName} -i ${filePath}`);
    } catch (err) {
        console.log(`Error formatting ${filePath}: ${err}`);
    }
}
