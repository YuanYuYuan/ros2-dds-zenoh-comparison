## This is a project to compare the performance of ROS2 point cloud message passing between native DDS and Zenoh.

* ROS Velodyne driver: https://github.com/ros-drivers/velodyne/tree/ros2


```mermaid
flowchart TB
subgraph SR[LiDAR Source]
    direction LR
    VL(Velodyne LiDAR)
    VD(velodyne_driver)
    VP(velodyne_pointcloud)
    VL --> VD --> VP
end
VP -- P0 --> TF
subgraph TF[Transfer]
    direction LR
    RP(ROS2 publisher)
    ZP(Zenoh publisher)
end
subgraph FRC[Frame Rate Counter]
    direction TB
    RS(ROS2 subscriber)
    ZS(Zenoh subscriber)
end
RP -- P1 --> RS
ZP -- P2 --> ZS
ZBD1(Zenoh Bridge DDS)
ZBD2(Zenoh Bridge DDS)
RP --> ZBD1 --> ZBD2 -- P3 --> RS
ZBD1 -- P4 --> ZS
```

Here's a simple visualizaton of the mechanism of [zenoh-bridge-dds](https://github.com/eclipse-zenoh/zenoh-plugin-dds).

```mermaid
flowchart LR
A{{ROS2 msg}} -- DDS --> B(Zenoh Bridge DDS) -- Zenoh --> C{{Zenoh msg}}
```

There're four pathways to transfer the raw LiDAR packets to the endpoint Frame rate counter.

* P0: Raw LiDAR packet from the physical LiDAR sensor
* P1: the default way used in ROS2, passing messages with DDS
* P2: replacing the last message passing by pure zenoh protocol
* P3: using two Zenoh/DDS bridges to cross the local network
* P4: directly using the zenoh message after Zenoh/DDS bridge


## Prerequisites

1. ROS2 Humble

Make sure you have [ROS2 humble](https://docs.ros.org/en/humble/Installation.html) installed on your system.

2. Download ROS dependencies
```bash
vcs import --input src/ros2.repos src
```

3. Build ROS packages

```bash
source /opt/ros/humble/setup.bash
colcon build
```


4. Download the sample data

![NAME](./pic/demo.gif)

```bash
wget https://github.com/YuanYuYuan/ros2-dds-zenoh-comparison/releases/download/2022-12-15/sample-data.tar.xz
tar xvf sample-data.tar.xz
```

5. (Optional) build zenoh router if needed
```
git clone https://github.com/eclipse-zenoh/zenoh
cd zenoh
cargo build --release --bin zenohd
```

## Usage

```bash
source ./env.bash
ros2 bag play sample-data/rosbag2_2022_12_09-21_10_35_0.db3 --loop -r 1
ros2 launch velodyne_pointcloud velodyne_transform_node-VLP32C-launch.py
ros2 launch comparison zenoh_pub.py
ros2 launch comparison zenoh_sub.py
rviz2 -f velodyne
```


## Tips

Setup ccls via cmake _compile_commands.json_.

```bash
colcon build --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -s ./build/compile_commands.json .
```

## Experiments

### P1 vs P2: Comparison between Zenoh and ROS2/DDS

1. For autonomous vehicles driving on the road, there is one case of single LiDAR source followed
by multiple perception and then gather into the planning node to make decision.
    ```mermaid
    flowchart LR
    SR[LiDAR Sensor]
    P1[Perception Model 1]
    P2[Perception Model 2]
    PN[Perception Model N]
    PL[Planning Node]
    SR ---> P1 --> PL
    SR ---> P2 --> PL
    SR ---> PN --> PL
    ```
2. To simulate this use case and to make a fair comparison between Zenoh and ROS2/DDS (CycloneDDS used here). We use the following workflow,
    ```mermaid
    flowchart LR
    P1[Data Generator 1]
    P2[Data Generator 2]
    PN[Data Generator N]
    PL[Frame rate counter]
    P1 -- Zenoh or DDS--> PL
    P2 -- Zenoh or DDS--> PL
    PN -- Zenoh or DDS--> PL
    ```
where `Data Generator` is used to simulate some large intermedidate data with adjustable payload size in practice.


3. The following script can be used to compare the performance between the paths P1 and P2.
    ```bash
    ./scripts/compare-pcd-gen.sh
    ```

    Visualize the results, e.g.

    ```bash
    ./scripts/plot.py --log ./logs/20230112.102116/65536 --title "P1 vs P2, payload: 64 KiB"
    ```

4. Results

* ROS2 DDS: CycloneDDS
* Payload size: 4 KiB to 4 MiB
* Number of data generating nodes: 1, 3, 5, ..., 19

* Data generating speed: 200 Hz per node

|                                               |                                                |
| -                                             | -                                              |
| ![NAME](./pic/results/200hz/P1-P2-4-KiB.png)  | ![NAME](./pic/results/200hz/P1-P2-16-KiB.png)  |
| ![NAME](./pic/results/200hz/P1-P2-64-KiB.png) | ![NAME](./pic/results/200hz/P1-P2-256-KiB.png) |
| ![NAME](./pic/results/200hz/P1-P2-1-MiB.png)  | ![NAME](./pic/results/200hz/P1-P2-4-MiB.png)   |

* Data generating speed: 20 Hz per node

|                                              |                                               |
| -                                            | -                                             |
| ![NAME](./pic/results/20hz/P1-P2-4-KiB.png)  | ![NAME](./pic/results/20hz/P1-P2-16-KiB.png)  |
| ![NAME](./pic/results/20hz/P1-P2-64-KiB.png) | ![NAME](./pic/results/20hz/P1-P2-256-KiB.png) |
| ![NAME](./pic/results/20hz/P1-P2-1-MiB.png)  | ![NAME](./pic/results/20hz/P1-P2-4-MiB.png)   |
