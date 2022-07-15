#pragma once
#include <glm/vec2.hpp>
#include <slib/List.hpp>
#include <string>

namespace worlds::nodes
{
    struct DataType
    {
        std::string name;
        uint32_t color;
    };

    struct Port
    {
        std::string name;
        DataType *type;
    };

    struct NodeType
    {
        std::string name;
        slib::List<Port> inPorts;
        slib::List<Port> outPorts;
    };

    struct Node
    {
        NodeType *type;
        glm::vec2 position;
        glm::vec2 size;
    };

    struct Connection
    {
        Node *from;
        Node *to;
        uint32_t fromPort;
        uint32_t toPort;
    };

    class NodeEditor
    {
      public:
        NodeEditor();
        void registerDataType(DataType *type);
        void registerNodeType(NodeType *type);
        void draw();
        void addConnection(Connection c);

      private:
        slib::List<Connection> connections;
        glm::vec2 scrollOffset;
    };
}