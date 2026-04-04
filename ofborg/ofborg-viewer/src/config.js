const infra = "projecttick.net";

const WELL_KNOWN = `https://logs.tickborg.${infra}/logs`;
const SOCK = `wss://logs.tickborg.${infra}/ws/`;
const SOCK_VHOST = `/`;
const AUTH = "tickborg-logviewer";
const MAX_LINES = 25000;

export {SOCK, SOCK_VHOST, AUTH, MAX_LINES, WELL_KNOWN};
