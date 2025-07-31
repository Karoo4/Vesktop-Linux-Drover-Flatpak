#!/bin/bash

# Installer script for Discord Drover with Vesktop Flatpak on Linux
# Usage: ./install.sh [install|uninstall]

set -e

VESTOP_CONFIG="$HOME/.var/app/dev.vencord.Vesktop/config"
LIB_DIR="$HOME/.var/app/dev.vencord.Vesktop/lib"
LIB_PATH="$LIB_DIR/libdrover.so"
CONFIG_PATH="$VESTOP_CONFIG/drover.ini"
WRAPPER_SCRIPT="$HOME/.local/bin/vesktop-drover"
DESKTOP_FILE="$HOME/.local/share/applications/dev.vencord.Vesktop.desktop"
DESKTOP_SRC="/var/lib/flatpak/exports/share/applications/dev.vencord.Vesktop.desktop"

# Compile the shared library
compile() {
    echo "Compiling drover.c..."
    if ! command -v gcc >/dev/null 2>&1; then
        echo "gcc is not installed. Installing gcc on CachyOS..."
        sudo pacman -S --needed gcc
    fi
    # Install necessary development libraries
    sudo pacman -S --needed glibc
    gcc -shared -fPIC -o libdrover.so drover.c -ldl
    mkdir -p "$LIB_DIR"
    mv libdrover.so "$LIB_PATH"
    chmod 644 "$LIB_PATH"
}

# Install function
install() {
    # Check if Vesktop Flatpak is installed
    if ! flatpak list | grep -q dev.vencord.Vesktop; then
        echo "Vesktop Flatpak not found. Installing it..."
        flatpak install --user flathub dev.vencord.Vesktop
    fi

    # Compile and install the library
    compile

    # Create default configuration
    mkdir -p "$VESTOP_CONFIG"
    if [ ! -f "$CONFIG_PATH" ]; then
        cat > "$CONFIG_PATH" << EOF
[drover]
proxy = http://127.0.0.1:1080
use-nekobox-proxy = 0
nekobox-proxy = http://127.0.0.1:2080
direct-mode = 1
log-file = /tmp/drover.log
EOF
        chmod 644 "$CONFIG_PATH"
        echo "Created configuration at $CONFIG_PATH"
    else
        echo "Configuration file $CONFIG_PATH already exists, skipping creation"
    fi

    # Grant Flatpak permissions
    flatpak override --user --filesystem="$LIB_DIR" dev.vencord.Vesktop
    flatpak override --user --env=LD_PRELOAD="$LIB_PATH" dev.vencord.Vesktop

    # Create wrapper script
    mkdir -p "$HOME/.local/bin"
    cat > "$WRAPPER_SCRIPT" << EOF
#!/bin/bash
flatpak run --env=LD_PRELOAD="$LIB_PATH" dev.vencord.Vesktop
EOF
    chmod +x "$WRAPPER_SCRIPT"
    echo "Created wrapper script at $WRAPPER_SCRIPT"

    # Modify desktop entry
    if [ -f "$DESKTOP_SRC" ]; then
        mkdir -p "$HOME/.local/share/applications"
        cp "$DESKTOP_SRC" "$DESKTOP_FILE"
        sed -i "s|^Exec=.*|Exec=$WRAPPER_SCRIPT|" "$DESKTOP_FILE"
        chmod 644 "$DESKTOP_FILE"
        echo "Modified desktop entry at $DESKTOP_FILE"
        update-desktop-database "$HOME/.local/share/applications" || echo "Failed to update desktop database, but installation can continue"
    else
        echo "Warning: Desktop file $DESKTOP_SRC not found, skipping desktop entry modification"
    fi

    echo "Installation complete!"
    echo "To run Vesktop with Drover, use: $WRAPPER_SCRIPT"
    echo "Or launch Vesktop from your application menu."
}

# Uninstall function
uninstall() {
    if [ -f "$LIB_PATH" ]; then
        rm "$LIB_PATH"
        rmdir --ignore-fail-on-non-empty "$LIB_DIR"
        echo "Removed $LIB_PATH"
    fi
    if [ -f "$CONFIG_PATH" ]; then
        rm "$CONFIG_PATH"
        echo "Removed $CONFIG_PATH"
    fi
    if [ -f "$WRAPPER_SCRIPT" ]; then
        rm "$WRAPPER_SCRIPT"
        echo "Removed $WRAPPER_SCRIPT"
    fi
    if [ -f "$DESKTOP_FILE" ]; then
        rm "$DESKTOP_FILE"
        update-desktop-database "$HOME/.local/share/applications" || true
        echo "Removed $DESKTOP_FILE"
    fi
    flatpak override --user --reset dev.vencord.Vesktop
    echo "Uninstalled Drover and reset Flatpak overrides"
}

# Main
case "$1" in
    install)
        install
        ;;
    uninstall)
        uninstall
        ;;
    *)
        echo "Usage: $0 [install|uninstall]"
        exit 1
        ;;
esac
