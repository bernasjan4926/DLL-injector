import ctypes
import tkinter as tk
from tkinter import messagebox, filedialog, ttk
import psutil
import traceback

class DLLInjector:
    def __init__(self, root):
        self.root = root
        self.root.title("Frosty-DLL-Injector")
        
        self.process_name = tk.StringVar()
        self.dll_path = tk.StringVar()
        
        # Process selection
        tk.Label(root, text="Select Process:").pack(pady=5)
        self.process_combo = ttk.Combobox(root, textvariable=self.process_name)
        self.process_combo.pack(pady=5)
        self.update_process_list()

        # DLL path entry
        tk.Label(root, text="DLL Path:").pack(pady=5)
        self.dll_entry = tk.Entry(root, textvariable=self.dll_path)
        self.dll_entry.pack(pady=5)

        # Load DLL button
        tk.Button(root, text="Load DLL", command=self.load_dll).pack(pady=5)
        
        # Inject button
        tk.Button(root, text="Inject DLL", command=self.inject_dll).pack(pady=5)

        # Active/Deactivate button
        self.active = False
        self.toggle_button = tk.Button(root, text="Activate", command=self.toggle)
        self.toggle_button.pack(pady=5)

    def update_process_list(self):
        process_names = [p.name() for p in psutil.process_iter()]
        self.process_combo['values'] = process_names
        if process_names:
            self.process_combo.current(0)  # Select the first process by default

    def load_dll(self):
        dll_file = filedialog.askopenfilename(title="Select DLL", filetypes=[("DLL files", "*.dll")])
        if dll_file:
            self.dll_path.set(dll_file)

    def inject_dll(self):
        process_name = self.process_combo.get()
        dll_path = self.dll_path.get()
        
        try:
            print("Looking for process:", process_name)
            process = next(p for p in psutil.process_iter() if p.name() == process_name)
            process_id = process.pid
            print("Found process ID:", process_id)

            # Open the process
            process_handle = ctypes.windll.kernel32.OpenProcess(0x1F0FFF, False, process_id)
            if not process_handle:
                error_code = ctypes.GetLastError()
                messagebox.showerror("Error", f"Could not open process. Error code: {error_code}.")
                print(f"OpenProcess failed with error code: {error_code}")
                return

            # Allocate memory for the DLL path
            dll_path_c = dll_path  # Keep it as a string
            dll_path_len = len(dll_path_c) + 1  # Include space for null terminator
            remote_memory = ctypes.windll.kernel32.VirtualAllocEx(process_handle, None, dll_path_len * 2, 0x1000 | 0x2000, 0x40)

            if not remote_memory:
                error_code = ctypes.GetLastError()
                messagebox.showerror("Error", f"Could not allocate memory. Error code: {error_code}.")
                print(f"VirtualAllocEx failed with error code: {error_code}")
                return

            # Write the DLL path to the remote process memory
            result = ctypes.windll.kernel32.WriteProcessMemory(process_handle, remote_memory, dll_path_c, dll_path_len * 2, None)
            if not result:
                error_code = ctypes.GetLastError()
                messagebox.showerror("Error", f"Could not write to process memory. Error code: {error_code}.")
                print(f"WriteProcessMemory failed with error code: {error_code}")
                return

            # Create a remote thread to inject the DLL
            thread_id = ctypes.windll.kernel32.CreateRemoteThread(process_handle, None, 0,
                ctypes.windll.kernel32.GetProcAddress(ctypes.windll.kernel32.GetModuleHandleW("kernel32.dll"), "LoadLibraryW"), 
                remote_memory, 0, None)
            
            if not thread_id:
                error_code = ctypes.GetLastError()
                messagebox.showerror("Error", f"Could not create remote thread. Error code: {error_code}.")
                print(f"CreateRemoteThread failed with error code: {error_code}")
                return
            
            messagebox.showinfo("Success", "DLL injected successfully.")
            print("DLL injected successfully.")

        except Exception as e:
            messagebox.showerror("Error", str(e))
            print("Exception occurred:", e)
            traceback.print_exc()

    def toggle(self):
        self.active = not self.active
        self.toggle_button.config(text="Deactivate" if self.active else "Activate")

if __name__ == "__main__":
    try:
        root = tk.Tk()
        injector = DLLInjector(root)
        print("Starting the injector GUI...")
        root.mainloop()
        print("Injector closed.")
    except Exception as e:
        print("An error occurred:", e)
        traceback.print_exc()