Sample init scripts and service configuration for bellsd
==========================================================

Sample scripts and configuration files for systemd, Upstart and OpenRC
can be found in the contrib/init folder.

    contrib/init/bellsd.service:    systemd service unit configuration
    contrib/init/bellsd.openrc:     OpenRC compatible SysV style init script
    contrib/init/bellsd.openrcconf: OpenRC conf.d file
    contrib/init/bellsd.conf:       Upstart service configuration file
    contrib/init/bellsd.init:       CentOS compatible SysV style init script

Service User
---------------------------------

All three Linux startup configurations assume the existence of a "bells" user
and group.  They must be created before attempting to use these scripts.
The macOS configuration assumes bellsd will be set up for the current user.

Configuration
---------------------------------

Running bellsd as a daemon does not require any manual configuration. You may
set the `rpcauth` setting in the `bells.conf` configuration file to override
the default behaviour of using a special cookie for authentication.

This password does not have to be remembered or typed as it is mostly used
as a fixed token that bellsd and client programs read from the configuration
file, however it is recommended that a strong and secure password be used
as this password is security critical to securing the wallet should the
wallet be enabled.

If bellsd is run with the "-server" flag (set by default), and no rpcpassword is set,
it will use a special cookie file for authentication. The cookie is generated with random
content when the daemon starts, and deleted when it exits. Read access to this file
controls who can access it through RPC.

By default the cookie is stored in the data directory, but its location can be
overridden with the option `-rpccookiefile`. Default file permissions for the
cookie are "owner" (i.e. user read/writeable) via default application-wide file
umask of `0077`, but these can be overridden with the `-rpccookieperms` option.

This allows for running bellsd without having to do any manual configuration.

`conf`, `pid`, and `wallet` accept relative paths which are interpreted as
relative to the data directory. `wallet` *only* supports relative paths.

To generate an example configuration file that describes the configuration settings,
see [contrib/devtools/README.md](../contrib/devtools/README.md#gen-bells-confsh).

Paths
---------------------------------

### Linux

All three configurations assume several paths that might need to be adjusted.

    Binary:              /usr/bin/bellsd
    Configuration file:  /etc/bells/bells.conf
    Data directory:      /var/lib/bellsd
    PID file:            /var/run/bellsd/bellsd.pid (OpenRC and Upstart) or
                         /run/bellsd/bellsd.pid (systemd)
    Lock file:           /var/lock/subsys/bellsd (CentOS)

The PID directory (if applicable) and data directory should both be owned by the
bells user and group. It is advised for security reasons to make the
configuration file and data directory only readable by the bells user and
group. Access to bells-cli and other bellsd rpc clients can then be
controlled by group membership.

NOTE: When using the systemd .service file, the creation of the aforementioned
directories and the setting of their permissions is automatically handled by
systemd. Directories are given a permission of 710, giving the bells group
access to files under it _if_ the files themselves give permission to the
bells group to do so. This does not allow
for the listing of files under the directory.

NOTE: It is not currently possible to override `datadir` in
`/etc/bells/bells.conf` with the current systemd, OpenRC, and Upstart init
files out-of-the-box. This is because the command line options specified in the
init files take precedence over the configurations in
`/etc/bells/bells.conf`. However, some init systems have their own
configuration mechanisms that would allow for overriding the command line
options specified in the init files (e.g. setting `BELLSD_DATADIR` for
OpenRC).

### macOS

    Binary:              /usr/local/bin/bellsd
    Configuration file:  ~/Library/Application Support/Bells/bells.conf
    Data directory:      ~/Library/Application Support/Bells
    Lock file:           ~/Library/Application Support/Bells/.lock

Installing Service Configuration
-----------------------------------

### systemd

Installing this .service file consists of just copying it to
/usr/lib/systemd/system directory, followed by the command
`systemctl daemon-reload` in order to update running systemd configuration.

To test, run `systemctl start bellsd` and to enable for system startup run
`systemctl enable bellsd`

NOTE: When installing for systemd in Debian/Ubuntu the .service file needs to be copied to the /lib/systemd/system directory instead.

### OpenRC

Rename bellsd.openrc to bellsd and drop it in /etc/init.d.  Double
check ownership and permissions and make it executable.  Test it with
`/etc/init.d/bellsd start` and configure it to run on startup with
`rc-update add bellsd`

### Upstart (for Debian/Ubuntu based distributions)

Upstart is the default init system for Debian/Ubuntu versions older than 15.04. If you are using version 15.04 or newer and haven't manually configured upstart you should follow the systemd instructions instead.

Drop bells.conf in /etc/init.  Test by running `service bellsd start`
it will automatically start on reboot.

NOTE: This script is incompatible with CentOS 5 and Amazon Linux 2014 as they
use old versions of Upstart and do not supply the start-stop-daemon utility.

### CentOS

Copy bellsd.init to /etc/init.d/bellsd. Test by running `service bellsd start`.

Using this script, you can adjust the path and flags to the bellsd program by
setting the BELLSD and FLAGS environment variables in the file
/etc/sysconfig/bellsd. You can also use the DAEMONOPTS environment variable here.

### macOS

Copy org.bells.bellsd.plist into ~/Library/LaunchAgents. Load the launch agent by
running `launchctl load ~/Library/LaunchAgents/org.bells.bellsd.plist`.

This Launch Agent will cause bellsd to start whenever the user logs in.

NOTE: This approach is intended for those wanting to run bellsd as the current user.
You will need to modify org.bells.bellsd.plist if you intend to use it as a
Launch Daemon with a dedicated bells user.

Auto-respawn
-----------------------------------

Auto respawning is currently only configured for Upstart and systemd.
Reasonable defaults have been chosen but YMMV.
