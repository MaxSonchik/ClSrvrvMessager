<div align="center">
<h1>⁺˚⋆｡°✩₊✩°｡⋆˚⁺ **ClSrvrvMessager** ⁺˚⋆｡°✩₊✩°｡⋆˚⁺</h1>
</div>
<div align="center">
  <img src="pictures/hacker-hacker-man.gif" alt="Hacker GIF" width="300">
</div>

**ClSrvrvMessager** is a C++ based client-server messenger application utilizing Boost.Asio. Key components include:
  - 🖥 **Server**: Listens for TCP connections, manages client registration, and relays text messages.
  - 👨🏻‍💻 **Client**: Performs STUN requests to determine its public IP/port, registers with the server (providing its public address), and exchanges text messages via TCP. File transfers use UDP with a basic block acknowledgment mechanism.
The project also incorporates a Python GUI for client interaction, Kubernetes deployment for scalability and container management, and an ML-based model for auto-scaling based on load.

<details>
<summary>Additional Information</summary>
  The project has a multi-file structure and uses a Makefile for building. The code adheres to fundamental requirements such as STUN packet alignment (RFC 5389), message serialization/deserialization, network error handling, and asynchronous I/O logic with Boost.Asio.
</details>

***
### **📄 Table of Contents**
1. Requirements
2. Technologies Used
3. Specifications
4. Core Features
5. Python GUI
6. Project Structure
7. Dockerization
8. Kubernetes Deployment
9. ML-based Auto-scaling
10. Boost Test Results
***

### **✅ Requirements**
- C++17 compatible compiler (GCC 7+ or Clang 5+ recommended).
- Boost.Asio and Boost.System libraries installed.
- Build tools (e.g., make).
- Internet connection (for STUN functionality).
***
### **🌐 Technologies Used**
- Programming Language: **C++17**
- Boost.Asio: For asynchronous network I/O (TCP and UDP).
- Boost.System: For system error handling and Boost.Asio integration.
- STUN (Session Traversal Utilities for NAT, RFC 5389): To determine the public IP/port of a client behind NAT.
- TCP & UDP: For reliable text messaging and signaling (TCP) and fast file transfer with basic ACK (UDP).
- Makefile: For simplifying the project build process.
***
### **📘 Specifications**
The solution uses multiple complementary protocols and standards to ensure reliable, convenient, and flexible communication, even in complex network conditions.

#### STUN (RFC 5389)

 <details>
  <summary>❓Quick primer on NAT before diving in❓</summary>
   NAT (Network Address Translation) is a technology widely used in routers and firewalls to map "internal" local network addresses to one or more public IP addresses. However, NAT complicates direct interaction between hosts located behind different NAT devices. When a client tries to establish a connection from the outside, it only sees the public address assigned by the router, not knowing the internal address of the host behind the NAT.
 </details>

The problem is that without knowing its public address, a host behind NAT cannot inform other network participants how to connect to it directly. If clients behind NAT simply exchanged their internal IP addresses (e.g., 192.168.x.x), they wouldn't be able to connect, as these addresses are not visible or routable on the global internet.
Our Solution:
To determine the public IP address and port of a client hidden behind NAT, we use the STUN protocol. Its key function is to help the client "see itself from the outside." By sending a Binding Request to a STUN server, the client receives its public address in response. This enables direct interaction between clients in different networks and allows flexible operation under various NAT configurations.
>- [❌] Simple local network implementation
>- [✅] Comprehensive project with STUN protocol implementation

#### **➤ TCP for Text Messages**

**TCP (Transmission Control Protocol)** is a reliable transport layer protocol used for data transmission, including text messages, between the client and server. TCP ensures guaranteed delivery of messages in the correct order.
How it works: For text messaging using TCP, the server opens a socket and listens for incoming connections, while the client establishes a connection and sends data. Messages are transmitted as byte streams, which are converted back to readable text on the receiving end.

#### **➤ UDP for File Transfer**
For file transfer, **UDP (User Datagram Protocol)** is used because it is:
- Faster, as it doesn't require establishing a reliable connection, reducing latency.
- More flexible, allowing implementation of custom block-based acknowledgment (ACK) mechanisms, adapting to specific speed or reliability requirements.

We don't use "raw" UDP. Instead, a simple protocol is implemented on top of it, sending data in small chunks and waiting for an ACK from the receiver. If an ACK is not received, the sender can resend the lost chunk. This approach balances performance and reliability without overly complex flow control and retransmission schemes.
<details>
  <summary>Visual illustration of UDP and TCP operation ⤵</summary>

  ![Работа UDP и TCP](pictures/photo_2024-12-20_10-46-07.jpg)

</details>

#### **➤ Boost Documentation and Boost.Asio Usage**
For network interaction, we used the Boost.Asio library, a well-known tool for asynchronous I/O in C++. Its key features:
- Asynchronous model: Efficiently uses resources and handles multiple connections.
- Versatility: Supports TCP, UDP, timers, and other I/O mechanisms through a unified interface.
- Documentation: The official [Boost.Asio Documentation](https://www.boost.org/doc/libs/master/doc/html/boost_asio.html) provides examples, tutorials, and API descriptions, facilitating learning and development.

During development, we relied on standard examples from the documentation and protocol descriptions (RFC 5389 for STUN, TCP/UDP specifications). This helped create a solution combining Boost.Asio's convenience and flexibility with the reliability of well-established network protocols.
***
### **🤖 Core Features**
The messenger integrates key elements for convenient communication beyond local networks. Before connecting, the client determines its public address using STUN, simplifying interaction when working through NAT. The server acts as a "hub" for all connections, redirecting text messages between users. A lightweight and fast UDP channel with a basic delivery confirmation mechanism is provided for file transfer. This approach ensures smooth data exchange and simplifies collaboration, making communication transparent and accessible.
***

### **🐍 Python GUI**
A Python-based Graphical User Interface (GUI) is provided for interacting with the messenger client. It allows users to send and receive messages in a user-friendly windowed application.

**Dependency Installation:**
Before running the GUI, install the required libraries. Ensure you have `pip` for Python installed. Then, execute the following command in the terminal from the project's root directory:
```bash
pip install -r src/GUI/requirements.txt
```

**Running the GUI:**
To start the GUI, execute the following command from the project's root directory:
```bash
python src/GUI/main.py
```
***
### **☰ Project Structure**
![Project Structure Diagram](pictures/схема.jpg)

Проект организован по классической схеме: в директории `src/` располагается исходный код, отвечающий за основную логику работы, а в `include/` — заголовочные файлы с интерфейсами и общими определениями.

Тут вы можете по подробнее узнать про элементы реализации ⤵:

**`src/`**:
<details>
  <summary>main_server.cpp</summary>
  Entry point for starting the server.
</details>
<details>
  <summary>main_client.cpp</summary>
  Entry point for starting the client.
</details>
<details>
  <summary>common.cpp</summary>
  Implementation of common functions (logging, auxiliary utilities).
</details>
<details>
  <summary>stun.cpp, stun_client.cpp</summary>
  STUN client implementation.
</details>
<details>
  <summary>tcp_server.cpp, tcp_client.cpp</summary>
  Server and client implementation for TCP message exchange.
</details>
<details>
  <summary>udp_file_sender.cpp, udp_file_receiver.cpp</summary>
  Implementation of UDP file sending and receiving.
</details>
<details>
  <summary>message.cpp</summary>
  Message serialization implementation.
</details>
<details>
  <summary>file_transfer_protocol.cpp</summary>
  File transfer protocol implementation.
</details>
<details>
  <summary>encryption.cpp</summary>
  XOR encryption implementation.
</details>
<details>
  <summary>database.cpp</summary>
  Description of database interaction.
</details>
<details>
  <summary>Makefile</summary>
  Project build script. Running `make` will generate executables for the server and client.
</details>

***

### **🐋 Dockerization**

A Dockerfile is a tool that automates the process of building and running a project in an isolated environment. This ensures its stable operation on different computers, regardless of the host's operating system and software versions.
How Docker works:
The Dockerfile installs necessary libraries and tools like *g++, CMake, Boost, and SQLite*, avoiding manual environment setup.
Project source files are copied into the container, allowing the project to be built using CMake.
The project is then built using `cmake` and `make` commands, creating executables for the server, client, and file transfer utilities.
After building, the project (server by default) is launched. Other components like the client or file transfer tools can also be run.

<details>
  <summary>❓Why use Docker❓</summary>
  
  - **Project and dependency isolation**: Docker creates a secure, isolated space for the project and its dependencies.
  - **Build automation**: Dockerfile acts like a "magic wand," automating the project build process.
  - **Application deployment**: Dockerfile allows direct application startup within the container, ensuring convenience and reliability.
  
</details>

This project uses two main Dockerfiles:
1.  **`src/app/Dockerfile.messenger`**: For the C++ messenger server and client.
2.  **`src/ml-scripts/Dockerfile.model`**: For the Python-based ML auto-scaling model.

**🛠️ Building and Running the Messenger Docker Image (`src/app/Dockerfile.messenger`)**

1.  **Navigate to the application directory:**
    ```bash
    cd src/app
    ```
    *(Adjust path if your Dockerfile.messenger is located elsewhere)*

2.  **Build the Docker image:**
    Replace `your-messenger-app` with your desired image name and tag (e.g., `clsrvrvmessager/messenger:latest`).
    ```bash
    docker build -f Dockerfile.messenger -t your-messenger-app .
    ```

3.  **Run the Messenger Server container:**
    This example runs the server and maps port 8080 on the host to port 8080 in the container. Adjust ports as needed.
    ```bash
    docker run -d -p 8080:8080 your-messenger-app
    ```
    The server should now be accessible (e.g., at `http://localhost:8080`).

**🛠️ Building and Running the ML Model Docker Image (`src/ml-scripts/Dockerfile.model`)**

1.  **Navigate to the ML scripts directory:**
    ```bash
    cd src/ml-scripts
    ```

2.  **Build the Docker image:**
    Replace `your-ml-model` with your desired image name and tag (e.g., `clsrvrvmessager/ml-model:latest`).
    ```bash
    docker build -f Dockerfile.model -t your-ml-model .
    ```

3.  **Run the ML Model container:**
    How you run this container will depend on its specific function (e.g., if it is a service, if it needs specific ports exposed, or if it is part of a larger system like Kubernetes where it is run as a deployment).
    Refer to the ML model's specific documentation or the `ml_scaler.py` script for how it is intended to be used. For example, if it is a service that needs a port:
    ```bash
    docker run -d -p 5001:5001 your-ml-model
    ```
    *(Adjust port `5001` as per the ML service's requirements.)*

***

### **☸️ Kubernetes Deployment**
For deploying, scaling, and managing the containerized ClSrvrvMessager application in a cluster, Kubernetes is used. The `src/k8s` directory contains all necessary Kubernetes manifest files.

**Key Deployable Components:**
- **Messenger Server:** The main messaging server (`messenger-deployment.yaml`, `messenger-service.yaml`).
- **ML Scaler:** Component for automatic scaling based on the ML model (`ml-scaler-deployment.yaml`).
- Other components such as ConfigMaps, Persistent Volume Claims, RBAC, and a CronJob for model retraining are also in this directory.

**Deployment Instructions:**
Components are deployed to a Kubernetes cluster using the `kubectl` utility. You can apply manifests individually or all at once.

To apply a specific manifest:
```bash
kubectl apply -f src/k8s/<filename>.yaml
```
For example, to deploy the messenger server:
```bash
kubectl apply -f src/k8s/messenger-deployment.yaml
kubectl apply -f src/k8s/messenger-service.yaml
```

To apply all manifests in the `src/k8s` directory (it's recommended to apply `namespace.yaml` and `persistent-volume-claims.yaml` first if used for the first time):
```bash
kubectl apply -k src/k8s 
```
*Note: `kubectl apply -k` is used if a `kustomization.yaml` file is present in the `src/k8s` directory. If not, apply files individually or using `kubectl apply -f src/k8s/` (note the trailing `/`, which might require a specific directory structure or kubectl version).*

**Load Testing with JMeter:**
The project also includes a configuration for load testing using JMeter. The file `src/k8s/jmeter-load-test.yaml` can be used to launch JMeter pods in the cluster, which will execute tests based on the `src/k8s/messenger_test.jmx` plan. This helps assess the performance and scalability of the deployed application.

***

### **🧠 ML-based Auto-scaling**
For intelligent resource management, the project implements an ML-based auto-scaling system. All scripts related to training and using ML models are in the `src/ml-scripts` directory.

**Model Purpose:**
The main goal of these ML models is to predict the load on the messaging server and automatically adjust allocated resources (e.g., number of pods in Kubernetes) to ensure optimal performance and resource economy. Models can analyze historical traffic data, CPU usage, and other metrics to predict future load.

**Model Training:**
The following scripts are provided for training or retraining models:
- `src/ml-scripts/train_local.py`: Used for initial model training on a local dataset (e.g., `local_training_data.csv`).
- `src/ml-scripts/retrain_model.py`: Can be used for periodic model retraining based on new data collected during application operation. This script can be automated, for example, using a CronJob in Kubernetes (see `src/k8s/model-retrainer-cronjob.yaml`).

**Using `ml_scaler.py` with Kubernetes:**
The `src/ml-scripts/ml_scaler.py` script contains logic to get predictions from the trained ML model and interact with the Kubernetes API to adjust the number of replicas of the corresponding Deployment (e.g., messenger-server). This script is a key component of `ml-scaler-deployment.yaml`, which deploys it as a separate service in Kubernetes. It periodically queries the model, gets current metrics, and makes scaling decisions.

**Containerizing ML Models:**
`src/ml-scripts/Dockerfile.model` is used to package ML models and their dependencies into an isolated environment. This allows for easy deployment and updating of ML components in Kubernetes or other environments.

***

### **👥 Boost Test Results**
![Boost Test Results](pictures/tests.jpg)

***
