#!/usr/bin/env python3
import os
import shutil
import sys
import logging
from pathlib import Path


def main() -> int:
    logging.basicConfig(
        level=logging.INFO,
        format="%(levelname)s: %(message)s",
    )
    # Compute source directory (parent of this script's directory, i.e. espressif__av_processor)
    script_path = Path(__file__).resolve()
    av_processor_dir = script_path.parent.parent

    if av_processor_dir.name != "espressif__av_processor":
        logging.error("Failed to resolve source directory. Expected 'espressif__av_processor', got: %s", av_processor_dir)
        return 1

    # Ensure source espressif__av_processor is under managed_components
    if av_processor_dir.parent.name != "managed_components":
        logging.error("This script must be run when espressif__av_processor is under 'managed_components'. Current path: %s", av_processor_dir)
        return 1

    # Read PROJECT_DIR environment variable
    project_dir_env = os.environ.get("PROJECT_DIR")
    if not project_dir_env:
        logging.error("PROJECT_DIR is not set. Example: export PROJECT_DIR=/path/to/project")
        return 1

    project_dir = Path(project_dir_env).resolve()
    if not project_dir.exists() or not project_dir.is_dir():
        logging.error("PROJECT_DIR does not exist or is not a directory: %s", project_dir)
        return 1

    # Target components directory
    components_dir = project_dir / "components"
    try:
        components_dir.mkdir(parents=True, exist_ok=True)
    except Exception as e:
        logging.error("Failed to create target directory: %s -> %s", components_dir, e)
        return 1

    # Destination espressif__av_processor path
    dest_dir = components_dir / "espressif__av_processor"

    # If destination exists, remove it before copying
    if dest_dir.exists():
        logging.info("Destination exists, removing: %s", dest_dir)
        try:
            shutil.rmtree(dest_dir)
        except Exception as e:
            logging.error("Failed to remove existing destination: %s -> %s", dest_dir, e)
            return 1

    # Perform copy
    logging.info("Copying %s -> %s", av_processor_dir, dest_dir)
    try:
        shutil.copytree(av_processor_dir, dest_dir)
    except Exception as e:
        logging.error("Copy failed: %s", e)
        return 1

    # Switch to a safe working directory to avoid deleting the running/current dir
    try:
        os.chdir(project_dir)
    except Exception as e:
        logging.error("Failed to change working directory: %s -> %s", project_dir, e)
        return 1

    # Remove source directory (parent av_processor)
    logging.info("Removing source directory: %s", av_processor_dir)
    try:
        shutil.rmtree(av_processor_dir)
    except Exception as e:
        logging.error("Failed to remove source directory: %s -> %s", av_processor_dir, e)
        logging.info("Copy completed, but source removal failed. Please clean up manually.")
        return 1

    # Update PROJECT_DIR/main/idf_component.yml to remove 'espressif/av_processor'
    idf_component_path = project_dir / "main" / "idf_component.yml"
    if idf_component_path.exists():
        try:
            content = idf_component_path.read_text(encoding="utf-8")
            lines = content.splitlines()
            new_lines = [line for line in lines if "espressif/av_processor" not in line]
            if len(new_lines) != len(lines):
                idf_component_path.write_text("\n".join(new_lines) + ("\n" if content.endswith("\n") else ""), encoding="utf-8")
                logging.info("Removed 'espressif/av_processor' from %s", idf_component_path)
            else:
                logging.info("No 'espressif/av_processor' entry found in %s", idf_component_path)
        except Exception as e:
            logging.error("Failed to update %s: %s", idf_component_path, e)
            return 1
    else:
        logging.info("idf_component.yml not found at %s, skipping update", idf_component_path)

    logging.info("Copied espressif__av_processor to PROJECT_DIR/components, removed original directory, and updated idf_component.yml.")
    return 0


if __name__ == "__main__":
    sys.exit(main())


