# Point Cloud Compression (PGC)

This project implements custom point cloud compression algorithms using predictive geometry encoding, based on the MPEG PCC TMC13 standard. The project includes three custom encoder implementations with different optimization strategies for LiDAR point cloud compression.

## 📁 Directory Structure

```
pgc/
├── source/
│   ├── mpeg-pcc-tmc13/          # MPEG TMC13 reference implementation (submodule)
│   │   ├── tmc3/                # Core TMC3 source code
│   │   ├── dependencies/        # External dependencies
│   │   ├── cfg/                 # Configuration files
│   │   └── build/               # Build directory
│   │
│   └── custom/                  # Custom encoder implementations
│       ├── CMakeLists.txt       # Top-level CMake configuration
│       ├── build/               # Custom build directory
│       │
│       ├── 01_ring/             # Ring-based grouping encoder
│       │   ├── CMakeLists.txt
│       │   ├── COMPATIBILITY_ANALYSIS.md
│       │   └── source/
│       │       ├── geometry_predictive_encoder.cpp
│       │       └── ply.cpp
│       │
│       ├── 02_xyz/              # Cartesian coordinate encoder
│       │   ├── CMakeLists.txt
│       │   ├── analysis_cartesian_angular_mode.md
│       │   └── source/
│       │       ├── geometry_predictive_encoder.cpp
│       │       └── ply.cpp
│       │
│       └── 03_group8/           # 8-laser grouping encoder
│           ├── CMakeLists.txt
│           └── source/
│               ├── geometry_predictive_encoder.cpp
│               └── ply.cpp
│
├── scripts/                     # Jupyter notebooks for workflow
│   ├── 01_prep_dataset.ipynb   # Dataset preparation
│   ├── 02_run_test.ipynb       # Run compression tests
│   ├── 03_parse_results.ipynb  # Parse encoder outputs
│   ├── 04_calc_metrics.ipynb   # Calculate PSNR and metrics
│   └── 05_plot.ipynb           # Visualize results
│
├── datasets/                    # Point cloud datasets (not in repo)
│   └── nuscenes/               # nuScenes dataset
│       └── v1.0-mini/
│           └── ply/            # PLY format point clouds
│
├── experiments/                 # Experiment outputs (not in repo)
│
├── docs/                        # Documentation
│
├── .gitignore                  # Git ignore rules
├── .gitmodules                 # Git submodule configuration
└── README.md                   # This file
```

## 🚀 Getting Started

### Prerequisites

- **CMake** (version 3.0 or higher)
- **C++11** compatible compiler (GCC, Clang, or MSVC)
- **Git** (for cloning and submodule management)
- **Python 3** with Jupyter (for running analysis scripts)

### Clone the Repository

```bash
# Clone the repository
git clone https://github.com/swnh/pgc.git
cd pgc

# Initialize and update the TMC13 submodule
git submodule update --init --recursive
```

## 🔨 Building

### Build Custom Encoders

Each custom implementation can be built independently:

#### Option 1: Build All Custom Encoders

```bash
cd source/custom
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

This will generate three executables:
- `01_ring_tmc3` - Ring-based grouping encoder
- `02_xyz_tmc3` - Cartesian coordinate encoder  
- `03_group8_tmc3` - 8-laser grouping encoder

#### Option 2: Build Individual Encoder

```bash
cd source/custom/01_ring  # or 02_xyz or 03_group8
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

### Build Original TMC13 (Optional)

```bash
cd source/mpeg-pcc-tmc13
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

## 📊 Custom Implementations

### 1. **01_ring** - Ring-Based Grouping Encoder
- Groups LiDAR points by laser ring number
- Optimized for multi-ring LiDAR sensors
- Parses `laserangle`, `ring`, or `ring_number` attributes from PLY files
- See `COMPATIBILITY_ANALYSIS.md` for detailed analysis

### 2. **02_xyz** - Cartesian Coordinate Encoder
- Implements Cartesian residual encoding in angular mode
- Handles XYZ coordinates directly for better precision
- See `analysis_cartesian_angular_mode.md` for implementation details

### 3. **03_group8** - 8-Laser Grouping Encoder
- Groups lasers into sets of 8 for optimized prediction
- Reduces prediction complexity while maintaining quality
- Configurable grouping size

## 🧪 Usage

### Basic Compression

```bash
# Example: Compress a point cloud using the ring-based encoder
./source/custom/build/01_ring_tmc3 \
  --mode=0 \
  --positionQuantizationScale=1 \
  --uncompressedDataPath=input.ply \
  --compressedStreamPath=output.bin \
  --reconstructedDataPath=reconstructed.ply
```

### Running the Complete Workflow

Use the Jupyter notebooks in the `scripts/` directory:

1. **01_prep_dataset.ipynb** - Prepare and organize your dataset
2. **02_run_test.ipynb** - Run compression tests on all encoders
3. **03_parse_results.ipynb** - Extract metrics from log files
4. **04_calc_metrics.ipynb** - Calculate PSNR and compression rates
5. **05_plot.ipynb** - Generate comparison plots

```bash
# Start Jupyter
cd scripts
jupyter notebook
```

## 📦 Dataset Setup

The `datasets/` directory is excluded from version control due to size constraints. To set up datasets:

1. Download the nuScenes dataset (or your preferred point cloud dataset)
2. Convert to PLY format if needed
3. Place in `datasets/nuscenes/v1.0-mini/ply/` (or adjust paths in scripts)
4. Use `01_prep_dataset.ipynb` to organize and prepare data

## 🔧 Configuration

Each encoder uses the same TMC13 configuration options. Key parameters:

- `--mode=0` - Encoding mode (0=encode, 1=decode)
- `--positionQuantizationScale` - Quantization precision
- `--predGeomAzimuthQuantization` - Azimuth quantization for angular mode
- `--predGeomThetaQuantization` - Theta quantization for angular mode

Refer to the TMC13 documentation for complete parameter list.

## 📈 Performance Metrics

The project calculates:
- **Compression Rate** - Original size / Compressed size
- **PSNR** - Peak Signal-to-Noise Ratio for geometry
- **Bits Per Point (BPP)** - Compressed bits / Number of points
- **Encoding/Decoding Time**

## 🤝 Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Open a Pull Request

## 📄 License

[Add license information here]

## 🙏 Acknowledgments

- Based on [MPEG PCC TMC13](https://github.com/MPEGGroup/mpeg-pcc-tmc13)
- Tested on nuScenes dataset

## 📚 References

- MPEG-I Part 9: Geometry-based Point Cloud Compression
- [TMC13 Documentation](https://github.com/MPEGGroup/mpeg-pcc-tmc13)

## 📧 Contact

For questions or issues, please open an issue on GitHub.
