# A Lightweight MapReduce Framework for Edge Computing

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A decentralized, fault-tolerant, and resource-aware MapReduce framework designed specifically for efficient data processing in heterogeneous edge computing environments.

## 🎯 Objective

This project aims to design and implement a lightweight MapReduce framework tailored for edge computing. These environments are characterized by resource-constrained, heterogeneous devices (like Raspberry Pis, smartphones, and IoT nodes) and unreliable network connectivity. The core goal is to enable efficient, decentralized data processing close to the data source, thereby reducing latency and network bandwidth consumption.

## 💡 Motivation

Traditional MapReduce frameworks like Apache Hadoop were architected for powerful, homogeneous server clusters in data centers. These heavyweight systems are ill-suited for the edge, where devices operate with limited processing power, memory, and intermittent connectivity. A re-imagined framework optimized for these conditions can significantly enhance edge intelligence and unlock new possibilities for real-time data analytics at the source.

## ✨ Key Features & Architecture

Our framework is being built from the ground up to tackle the unique challenges of edge computing.

####  decentralised Decentralized Coordination
- **No Single Point of Failure:** Instead of a fixed master node, a master is dynamically elected from available edge nodes.
- **State Replication:** Critical cluster state and job progress are replicated across standby masters to ensure seamless recovery in case of master failure.

#### 🛠️ Resource-Aware Scheduling
- **Heterogeneity Management:** The framework dynamically profiles the capabilities (CPU, memory, network) of each worker node.
- **Intelligent Task Assignment:** The scheduler uses these profiles to assign Map and Reduce tasks to the most suitable nodes, optimizing overall job execution time and resource utilization.

#### 🚀 Efficient Data Movement
- **Reduced Network Load:** In-mapper combiners are used to perform early-stage aggregation, significantly reducing the amount of data that needs to be shuffled across the network.
- **Push-Based Shuffle:** Intermediate data is actively pushed from Map workers to Reduce workers, which is more efficient in environments with unpredictable network conditions than the pull-based model used in traditional systems.

#### 🛡️ Robust Fault Tolerance
- **Active Failure Detection:** A lightweight heartbeat mechanism allows the master to quickly detect worker node failures or unresponsiveness.
- **Automatic Task Reassignment:** Lost or failed tasks are automatically re-queued and reassigned to healthy workers without compromising the progress of the entire job.


