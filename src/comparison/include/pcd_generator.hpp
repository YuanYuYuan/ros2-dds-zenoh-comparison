#ifndef COMPARISON_PCD_GENERATOR
#define COMPARISON_PCD_GENERATOR

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <rclcpp/serialization.hpp>
#include "zenohcpp.h"
#include <chrono>


namespace pcd_generator {
    using PC2 = sensor_msgs::msg::PointCloud2;

    class BasePcdGenerator : public rclcpp::Node {
    public:
        BasePcdGenerator(std::string node_name) : rclcpp::Node(node_name) {
            // setup default parameters
            this->declare_parameter("pub_topic", "transfer_topic");
            this->declare_parameter("interval_ms", 5);
            this->declare_parameter("payload_size", 1024);

            // load given parameters
            this->pub_topic = this->get_parameter("pub_topic").as_string();
            this->interval_ms = this->get_parameter("interval_ms").as_int();
            this->payload_size = this->get_parameter("payload_size").as_int();

            // dummy message
            this->dummy_msg = std::make_shared<PC2>();
            this->dummy_msg->data = std::vector<uint8_t>(this->payload_size);

            this->timer = this->create_wall_timer(
                std::chrono::milliseconds(this->interval_ms),
                std::bind(&BasePcdGenerator::send_dummy_data, this)
            );
        }
        std::shared_ptr<PC2> dummy_msg;
        rclcpp::Serialization<PC2> serializer;
        rclcpp::QoS qos = rclcpp::QoS(rclcpp::KeepAll()).reliable();
        std::string pub_topic;
        int interval_ms;
        int payload_size;
        uint8_t message_header_length = 8u;
        virtual void send_dummy_data() const {};

    private:
        rclcpp::TimerBase::SharedPtr timer;
    };

    class RosPcdGenerator : public BasePcdGenerator {
    public:
        RosPcdGenerator() : BasePcdGenerator("ros_pcd_gen") {
            this->pub = this->create_publisher<PC2>(this->pub_topic, this->qos);
        }

    private:
        rclcpp::Publisher<PC2>::SharedPtr pub;
        void send_dummy_data() const {

            // serialize message
            auto message_payload_length = static_cast<size_t>(this->dummy_msg->data.size());
            rclcpp::SerializedMessage serialized_msg;
            serialized_msg.reserve(message_header_length + message_payload_length);
            serializer.serialize_message(this->dummy_msg.get(), &serialized_msg);

            // publish the serialized_msg
            this->pub->publish(serialized_msg);
        }

    };

    class ZenohPcdGenerator : public BasePcdGenerator {
    public:
        ZenohPcdGenerator() : BasePcdGenerator("zenoh_pcd_gen") {
            // open zenoh session
            zenoh::Config config;
            this->session = std::make_unique<zenoh::Session>(std::get<zenoh::Session>(zenoh::open(std::move(config))));

            // zenoh pub
            zenoh::PublisherOptions options;
            options.set_congestion_control(zenoh::CongestionControl::Z_CONGESTION_CONTROL_BLOCK);
            auto publisher = std::get<zenoh::Publisher>(this->session->declare_publisher(pub_topic.c_str(), options));
            this->pub = std::make_unique<zenoh::Publisher>(std::move(publisher));
        }

    private:
        std::unique_ptr<zenoh::Session> session;
        std::unique_ptr<zenoh::Publisher> pub;

        void send_dummy_data() const {
            // serialize message
            auto message_payload_length = static_cast<size_t>(this->dummy_msg->data.size());
            rclcpp::SerializedMessage serialized_msg;
            serialized_msg.reserve(this->message_header_length + message_payload_length);
            this->serializer.serialize_message(this->dummy_msg.get(), &serialized_msg);

            // zenoh put message
            auto inner_msg = serialized_msg.get_rcl_serialized_message();
            auto payload = zenoh::BytesView(inner_msg.buffer, inner_msg.buffer_length);
            pub->put(payload);
        }

    };

}

#endif
