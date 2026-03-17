var log = function(m) { document.getElementById("log").textContent += m + "\n"; };
var logBox = document.getElementById("log");
var progress = document.getElementById("progress");
var drop = document.getElementById("drop");

drop.addEventListener("dragover", function(ev) { ev.preventDefault(); drop.style.borderColor="#0f0"; });
drop.addEventListener("dragleave", function(ev) { drop.style.borderColor="#0f0"; });
drop.addEventListener("drop", function(ev) {
    ev.preventDefault();
    drop.style.borderColor="#0f0";
    var file = ev.dataTransfer.files[0];
    compressFile(file);
});

function compressFile(f) {
    log("Loaded: " + f.name + " (" + f.size + " bytes)");
    var reader = new FileReader();
    reader.onload = function(ev) {
        var buf = ev.target.result;
        var bytes = new Uint8Array(buf);

        // simple JS compression (LZ-like)
        log("Compressing...");
        progress.value = 0;

        var compressed = [];
        var last = bytes[0];
        var count = 1;

        for (var i = 1; i < bytes.length; i++) {
            progress.value = i / bytes.length * 100;
            if (bytes[i] === last && count < 255) {
                count++;
            } else {
                compressed.push(count);
                compressed.push(last);
                last = bytes[i];
                count = 1;
            }
        }
        compressed.push(count);
        compressed.push(last);

        var outBytes = compressed;
        log("Original: " + bytes.length + " bytes");
        log("Compressed: " + outBytes.length + " bytes");

        var outName = f.name + ".rawr";
        window.external.save(outName, outBytes);
        log("Saved: " + outName);
        progress.value = 100;
    };
    reader.readAsArrayBuffer(f);
}