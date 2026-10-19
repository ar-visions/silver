// ELECTRON_RUN_AS_NODE: plain node, but the program is quark
if (process.env.QUARK_LAUNCHER) process.execPath = process.env.QUARK_LAUNCHER;
process.versions.electron = '43.7.7';
process.versions.chrome = '0';
