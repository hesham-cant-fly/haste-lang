process.stdin.setEncoding("utf8");

let input = "";

process.stdin.on("data", chunk => {
    input += chunk;
});

process.stdin.on("end", () => {
    console.log(get_fixed(input));
});

function get_fixed(input) {
    const object = eval(`(${input})`);
    return JSON.stringify(object, undefined, 2);
}

