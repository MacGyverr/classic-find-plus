# GNU Make configuration without a Python build dependency. PREFIX is environment data.
function quoted(value,    i, c, out) {
    out = "\""
    for (i = 1; i <= length(value); i++) {
        c = substr(value, i, 1)
        if (c == "\\" || c == "\"") out = out "\\"
        if (c == "\n") out = out "\\n"
        else if (c == "\r") out = out "\\r"
        else out = out c
    }
    return out "\""
}
NR == 1 {
    prefix = ENVIRON["PREFIX"]
    if (mode == "service") {
        print "[D-BUS Service]"
        print "Name=org.classicfind.Plus.Search"
        print "Exec=" quoted(prefix "/libexec/classic-find-plus-search-worker")
        print "User=root"
    } else {
        print "#define CF_APP_VERSION " quoted($0)
        print "#define CF_INTEGRATION_HELPER " quoted(prefix "/share/classic-find-plus/integration-manager.py")
        print "#define CF_ACCOUNTS_HELPER " quoted(prefix "/share/classic-find-plus/accounts.py")
    }
}
