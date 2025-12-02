import os

# ===== CONFIG =====
dataset_path = r"C:\Users\admin\Downloads\images\images\disposable_plastic_cutlery\cutlery"  # Use raw string for Windows paths

# Recursive function to rename images in all subfolders
def rename_images_recursive(folder_path):
    for root, dirs, files in os.walk(folder_path):
        # Determine the label name from the folder (last part of path)
        label = os.path.basename(root)
        # Filter only files (images)
        image_files = [f for f in files if os.path.isfile(os.path.join(root, f))]
        image_files.sort()  # Optional: sort alphabetically

        # Rename images sequentially
        for idx, file_name in enumerate(image_files, start=1):
            file_ext = os.path.splitext(file_name)[1]  # Keep original extension
            new_name = f"{label}_{idx:03d}{file_ext}"  # e.g., cat_001.jpg
            old_file = os.path.join(root, file_name)
            new_file = os.path.join(root, new_name)
            os.rename(old_file, new_file)
            print(f"Renamed {old_file} -> {new_file}")

# Run the renaming
rename_images_recursive(dataset_path)
print("All images in subfolders have been renamed!")
