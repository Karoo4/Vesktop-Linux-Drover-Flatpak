import os
import subprocess
import tkinter as tk
import webbrowser
from tkinter import messagebox

GITHUB_URL = "https://github.com/Karoo4/Vesktop-Linux-Drover-Flatpak.git"


class DroverGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Discord Drover")
        self.root.geometry("600x550")
        self.root.resizable(False, False)

        # Initialize state
        self.mode = tk.StringVar(value="direct")
        self.hostname = tk.StringVar(value="127.0.0.1")
        self.port = tk.StringVar(value="1080")
        self.proxy_url = tk.StringVar(value="")
        self.auth_enabled = tk.BooleanVar(value=False)
        self.login = tk.StringVar(value="")
        self.password = tk.StringVar(value="")

        # Mode selection buttons
        self.mode_frame = tk.Frame(root)
        self.mode_frame.pack(pady=15)
        tk.Button(
            self.mode_frame,
            text="HTTP",
            command=lambda: self.set_mode("http"),
            width=10,
            bg="lightblue",
        ).pack(side=tk.LEFT, padx=10)
        tk.Button(
            self.mode_frame,
            text="SOCKS5",
            command=lambda: self.set_mode("socks5"),
            width=10,
            bg="lightblue",
        ).pack(side=tk.LEFT, padx=10)
        tk.Button(
            self.mode_frame,
            text="Direct",
            command=lambda: self.set_mode("direct"),
            width=10,
            bg="lightblue",
        ).pack(side=tk.LEFT, padx=10)

        # Input fields
        self.form_frame = tk.Frame(root)
        self.form_frame.pack(pady=10, padx=15, fill=tk.X)

        tk.Label(self.form_frame, text="Hostname", font=("Arial", 10)).pack(anchor="w")
        self.hostname_entry = tk.Entry(
            self.form_frame, textvariable=self.hostname, font=("Arial", 10)
        )
        self.hostname_entry.pack(fill=tk.X, pady=2)
        self.hostname_entry.bind("<KeyRelease>", self.update_proxy_url)

        tk.Label(self.form_frame, text="Port Number", font=("Arial", 10)).pack(
            anchor="w"
        )
        self.port_entry = tk.Entry(
            self.form_frame, textvariable=self.port, font=("Arial", 10)
        )
        self.port_entry.pack(fill=tk.X, pady=2)
        self.port_entry.bind("<KeyRelease>", self.update_proxy_url)

        tk.Label(self.form_frame, text="Proxy URL", font=("Arial", 10)).pack(anchor="w")
        self.proxy_url_entry = tk.Entry(
            self.form_frame,
            textvariable=self.proxy_url,
            state="readonly",
            font=("Arial", 10),
        )
        self.proxy_url_entry.pack(fill=tk.X, pady=2)

        self.auth_check = tk.Checkbutton(
            self.form_frame,
            text="Enable Authentication",
            variable=self.auth_enabled,
            command=self.toggle_auth,
            font=("Arial", 10),
        )
        self.auth_check.pack(anchor="w", pady=5)

        tk.Label(self.form_frame, text="Login", font=("Arial", 10)).pack(anchor="w")
        self.login_entry = tk.Entry(
            self.form_frame, textvariable=self.login, font=("Arial", 10)
        )
        self.login_entry.pack(fill=tk.X, pady=2)
        self.login_entry.bind("<KeyRelease>", self.update_proxy_url)

        tk.Label(self.form_frame, text="Password", font=("Arial", 10)).pack(anchor="w")
        self.password_entry = tk.Entry(
            self.form_frame, textvariable=self.password, show="*", font=("Arial", 10)
        )
        self.password_entry.pack(fill=tk.X, pady=2)
        self.password_entry.bind("<KeyRelease>", self.update_proxy_url)

        # Action buttons
        self.button_frame = tk.Frame(root)
        self.button_frame.pack(pady=20, fill=tk.X)
        tk.Button(
            self.button_frame,
            text="Install",
            command=self.install,
            bg="green",
            fg="white",
            width=12,
            font=("Arial", 10),
        ).grid(row=0, column=0, padx=10)
        tk.Button(
            self.button_frame,
            text="Uninstall",
            command=self.uninstall,
            bg="red",
            fg="white",
            width=12,
            font=("Arial", 10),
        ).grid(row=0, column=1, padx=10)
        tk.Button(
            self.button_frame,
            text="View on GitHub",
            command=self.view_github,
            bg="gray",
            fg="white",
            width=15,
            font=("Arial", 10),
        ).grid(row=0, column=2, padx=10)

        # Initial state
        self.update_proxy_url()
        self.toggle_auth()

    def set_mode(self, mode):
        self.mode.set(mode)
        self.update_proxy_url()
        self.toggle_auth()

    def update_proxy_url(self, event=None):
        if self.mode.get() == "direct":
            self.proxy_url.set("")
        else:
            auth_part = (
                f"{self.login.get()}:{self.password.get()}@"
                if self.auth_enabled.get() and self.login.get() and self.password.get()
                else ""
            )
            self.proxy_url.set(
                f"{self.mode.get()}://{auth_part}{self.hostname.get()}:{self.port.get()}"
            )

    def toggle_auth(self):
        is_direct = self.mode.get() == "direct"
        is_auth_enabled = self.auth_enabled.get() and not is_direct
        self.hostname_entry.config(state="disabled" if is_direct else "normal")
        self.port_entry.config(state="disabled" if is_direct else "normal")
        self.auth_check.config(state="disabled" if is_direct else "normal")
        self.login_entry.config(state="normal" if is_auth_enabled else "disabled")
        self.password_entry.config(state="normal" if is_auth_enabled else "disabled")
        self.update_proxy_url()

    def generate_config(self):
        return (
            "[drover]\n"
            f"proxy = {self.proxy_url.get()}\n"
            "use-nekobox-proxy = 0\n"
            "nekobox-proxy = http://127.0.0.1:2080\n"
            f"direct-mode = {1 if self.mode.get() == 'direct' else 0}\n"
            "log-file = /tmp/drover.log"
        )

    def install(self):
        try:
            # Write drover.ini
            config_dir = os.path.expanduser("~/.var/app/dev.vencord.Vesktop/config")
            config_path = os.path.join(config_dir, "drover.ini")
            os.makedirs(config_dir, exist_ok=True)
            with open(config_path, "w") as f:
                f.write(self.generate_config())
            messagebox.showinfo("Success", f"Saved drover.ini to {config_path}")

            # Run install.sh
            if os.path.exists("install.sh"):
                subprocess.run(["bash", "install.sh", "install"], check=True)
                messagebox.showinfo(
                    "Success",
                    "Installation completed! Run Vesktop with ~/.local/bin/vesktop-drover",
                )
            else:
                messagebox.showerror(
                    "Error", "install.sh not found in current directory"
                )
        except Exception as e:
            messagebox.showerror("Error", f"Installation failed: {str(e)}")

    def uninstall(self):
        try:
            if os.path.exists("install.sh"):
                subprocess.run(["bash", "install.sh", "uninstall"], check=True)
                messagebox.showinfo("Success", "Uninstallation completed")
            else:
                messagebox.showerror(
                    "Error", "install.sh not found in current directory"
                )
        except Exception as e:
            messagebox.showerror("Error", f"Uninstallation failed: {str(e)}")

    def view_github(self):
        if GITHUB_URL.startswith("http://") or GITHUB_URL.startswith("https://"):
            webbrowser.open(GITHUB_URL)
        else:
            messagebox.showerror("Error", "Invalid GitHub URL in code")


if __name__ == "__main__":
    root = tk.Tk()
    app = DroverGUI(root)
    root.mainloop()
