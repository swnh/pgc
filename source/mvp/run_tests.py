#!/usr/bin/env python3
import os
import subprocess
import glob
import re
import sys

def main():
    print("========================================")
    print(" Compiling SystemC Simulation")
    print("========================================")
    
    # Compile the simulation
    compile_cmd = [
        "g++", 
        "-I/home/swnh/systemc/src", 
        "-L/home/swnh/systemc/build/src", 
        "-lsystemc", 
        "-DSC_ALLOW_DEPRECATED_IEEE_API",
        "main.cc", "delta.cc", "binarizer.cc", "tb.cc", "context_modeler.cc", "arith_encoder.cc", "chunk_mux.cc",
        "-o", "sim"
    ]
    
    result = subprocess.run(compile_cmd)
    if result.returncode != 0:
        print("Compilation failed!")
        sys.exit(1)
        
    print("Compilation successful.\n")
    
    # Set environment variables for running the binary
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = "/home/swnh/systemc/build/src:" + env.get("LD_LIBRARY_PATH", "")

    # Discover files
    print("Discovering test files...")
    search_pattern = "/home/swnh/pgc/experiments/golden_ref_dump/**/*_in.csv"
    input_files = glob.glob(search_pattern, recursive=True)[:50]
    input_files.sort()
    
    total_files = len(input_files)
    print(f"Found {total_files} frames to test.\n")
    
    if total_files == 0:
        print("No test files found. Exiting.")
        sys.exit(0)

    passed = 0
    failed = 0
    failed_scenes = []

    print("========================================")
    print(" Running Simulations")
    print("========================================")

    error_regex = re.compile(r"Errors:\s+(\d+)")

    for idx, in_file in enumerate(input_files):
        # Infer the result file path
        res_file = in_file.replace("_in.csv", "_res.csv")
        scene_name = os.path.basename(in_file).replace("_in.csv", "")
        
        if not os.path.exists(res_file):
            print(f"[{idx+1:3d}/{total_files}] {scene_name}: SKIPPED (missing _res.csv)")
            continue

        # Run simulation
        run_cmd = ["./sim", in_file, res_file]
        try:
            # We capture stdout and stderr to parse the result without flooding the screen
            proc = subprocess.run(run_cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=60)
            
            # Parse output for errors
            output = proc.stdout
            match = error_regex.search(output)
            
            if match:
                errors = int(match.group(1))
                if errors == 0:
                    print(f"[{idx+1:3d}/{total_files}] {scene_name}: PASSED")
                    passed += 1
                else:
                    print(f"[{idx+1:3d}/{total_files}] {scene_name}: FAILED ({errors} errors)")
                    failed += 1
                    failed_scenes.append(scene_name)
                    # Save failed output to a log file
                    with open(f"{scene_name}_failed.log", "w") as f:
                        f.write(output)
            else:
                print(f"[{idx+1:3d}/{total_files}] {scene_name}: FAILED (could not parse output)")
                failed += 1
                failed_scenes.append(scene_name)
                # Save failed output to a log file
                with open(f"{scene_name}_failed.log", "w") as f:
                    f.write(output)
                
        except subprocess.TimeoutExpired:
            print(f"[{idx+1:3d}/{total_files}] {scene_name}: TIMEOUT")
            failed += 1
            failed_scenes.append(scene_name)

    print("\n========================================")
    print(" Test Summary")
    print("========================================")
    print(f"Total Tests : {total_files}")
    print(f"Passed      : {passed}")
    print(f"Failed      : {failed}")
    
    if failed > 0:
        print("\nFailed Scenes:")
        for scene in failed_scenes:
            print(f"  - {scene}")
        sys.exit(1)
    else:
        print("\nAll tests passed successfully!")
        sys.exit(0)

if __name__ == "__main__":
    main()
