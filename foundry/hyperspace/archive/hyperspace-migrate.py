import os
import json

# do not run this unless you know what do
exit(2)

def migrate_json_files(folder_path):
    """Convert JSON annotation format in a given folder."""
    for filename in os.listdir(folder_path):
        if filename.endswith(".json"):
            file_path = os.path.join(folder_path, filename)

            # Read the JSON file
            with open(file_path, "r", encoding="utf-8") as f:
                data = json.load(f)

            # Convert labels from list of objects to a single dictionary
            if "labels" in data and isinstance(data["labels"], list):
                new_labels = {}

                for annot in data["labels"]:
                    if isinstance(annot, dict):
                        new_labels.update(annot)  # Merge all key-value pairs into one dict

                # If no annotations exist, set labels to null
                data["labels"] = new_labels if new_labels else None

                # Overwrite the JSON file with the new structure
                with open(file_path, "w", encoding="utf-8") as f:
                    json.dump(data, f, indent=4)

                print(f"Updated: {filename}")

if __name__ == "__main__":
    f = 'sessions/center-close' # migrated the json format to be reduced and far simpler, and now proceeding to fix the -annotate and -record
    if os.path.isdir(f):
        migrate_json_files(f)
        print("Migration completed.")
    else:
        print("Invalid folder path.")
