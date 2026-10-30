python () {
    for name in ("X570D4U_UPSTREAM_REVISION", "X570D4U_BUILD_TIMESTAMP"):
        if not (d.getVar(name) or "").strip():
            bb.fatal("%s must be set for the X570D4U firmware version" % name)
}

DISTRO_VERSION = "${X570D4U_UPSTREAM_REVISION}-x570d4u-${X570D4U_BUILD_TIMESTAMP}"
EXTENDED_VERSION = "${DISTRO_VERSION}"
