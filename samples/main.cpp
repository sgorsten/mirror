#include "editor.h"
#include "json.h"

#include <GL/freeglut.h>

GraphEditor editor;

int g_editorGlutWindow, g_sketchpadGlutWindow;

int GetStringWidth12(const std::string & text)
{
    int w = 0;
    for(auto ch : text) w += glutBitmapWidth(GLUT_BITMAP_HELVETICA_12, ch);
    return w;
}

int GetStringWidth18(const std::string & text)
{
    int w = 0;
    for(auto ch : text) w += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, ch);
    return w;
}

void OnIdle() 
{ 
    glutPostRedisplay(); 
}

void OnMotion(int x, int y)
{
    int2 mousePos = {x,y}, delta = mousePos - editor.mouseover.coord;
    editor.mouseover = editor.GetFeature(mousePos);

    switch(editor.mode)
    {
    case GraphEditor::NewNodePopup:
        editor.mouseover = {};
        break;
    case GraphEditor::Dragging:
        switch(editor.clicked.type)
        {
        case Feature::Body:
            for(auto & node : editor.nodes)
            {
                if(node.selected)
                {
                    node.position += delta;
                }
            }
            break;
        }
        break;
    }
}

void OnMouse(int button, int state, int x, int y)
{
    switch(button)
    {
    case GLUT_LEFT_BUTTON:
        if(state == GLUT_DOWN)
        {
            switch(editor.mode)
            {
            case GraphEditor::NewNodePopup:
                {
                    int index = (y - editor.menuPos.y) / 16;
                    if(index >= 0 && index < editor.nodeTypes.size())
                    {
                        editor.nodes.push_back(Node(editor.menuPos, &editor.nodeTypes[index]));
                    }
                    editor.mode = GraphEditor::None;
                }
                break;
            case GraphEditor::None:
                editor.clicked = editor.mouseover;
                editor.mode = GraphEditor::Dragging;
                if(editor.clicked.type == Feature::Input)
                {
                    editor.clicked.node->inputs[editor.clicked.pin] = {-1,-1};
                }

                if(editor.clicked.type != Feature::Body && (glutGetModifiers() & GLUT_ACTIVE_SHIFT) == 0)
                {
                    for(auto & n : editor.nodes) n.selected = false;
                }

                if(editor.clicked.type == Feature::Body)
                {
                    if(glutGetModifiers() & GLUT_ACTIVE_CTRL)
                    {
                        editor.clicked.node->selected = !editor.clicked.node->selected;
                    }
                    else
                    {
                        if(!editor.clicked.node->selected)
                        {
                            for(auto & n : editor.nodes) n.selected = false;
                            editor.clicked.node->selected = true;
                        }
                    }
                }
                break;
            }
        }
        else // Mouse up
        {
            switch(editor.mode)
            {
            case GraphEditor::Dragging:
                if(editor.clicked.type == Feature::None) // Box selection
                {
                    Rect selectRect = { editor.clicked.coord, {x,y} };
                    if(selectRect.b0.x > selectRect.b1.x) std::swap(selectRect.b0.x, selectRect.b1.x);
                    if(selectRect.b0.y > selectRect.b1.y) std::swap(selectRect.b0.y, selectRect.b1.y);
      
                    for(auto & node : editor.nodes)
                    {
                        auto nodeRect = node.GetNodeRect();
                        if(nodeRect.b0.x > selectRect.b1.x) continue;
                        if(nodeRect.b1.x < selectRect.b0.x) continue;
                        if(nodeRect.b0.x > selectRect.b1.y) continue;
                        if(nodeRect.b1.x < selectRect.b0.y) continue;
                        node.selected = true;
                    }
                }
                else editor.ConnectPins(editor.clicked, editor.mouseover); // Wire connection
                break;
            }
            editor.mode = GraphEditor::None;
            editor.clicked = {};
        }
        break;
    case GLUT_RIGHT_BUTTON:
        if(state == GLUT_DOWN)
        {
            if(editor.mouseover.type == Feature::None)
            {
                editor.menuPos = {x,y};
                editor.mode = GraphEditor::NewNodePopup;
            }
            else if(editor.mouseover.type == Feature::Body)
            {
                auto type = editor.mouseover.node->nodeType;
                if(type->hasOutFlow && !type->hasInFlow) // Event!
                {
                    glutSetWindow(g_sketchpadGlutWindow);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    EventExecutionRecord record(editor.nodes);
                    record.ExecuteEvent(editor.mouseover.node - editor.nodes.data());

                    glutSwapBuffers();
                    glutSetWindow(g_editorGlutWindow);
                }
            }
        }
        break;
    }
}

#include <iostream>

void OnKeyboard(unsigned char key, int x, int y)
{
    if(editor.clicked.type == Feature::Input)
    {
        auto & im = editor.clicked.node->inputs[editor.clicked.pin].immediate;
        if(isprint(key)) im.push_back(key);
    }

    switch(key)
    {
    case 127: editor.DeleteSelection(); break;
    case 'p':
        std::cout << "[\n";
        for(size_t i=0; i<editor.nodes.size(); ++i)
        {
            const auto & n = editor.nodes[i];
            if(i) std::cout << ",\n";
            std::cout << "  {";
            std::cout << "\n    \"x\":" << n.position.x;
            std::cout << ",\n    \"y\":" << n.position.y;
            std::cout << ",\n    \"id\":\"" << n.nodeType->GetUniqueId() << "\"";
            if(!n.inputs.empty())
            {
                std::cout << ",\n    \"wires\":[\n";
                for(size_t j=0; j<n.inputs.size(); ++j)
                {
                    if(j) std::cout << ",\n";
                    if(n.inputs[j].nodeIndex == -1)
                    {
                        if(n.inputs[j].immediate.empty()) std::cout << "      null";
                        else std::cout << "      \"" << n.inputs[j].immediate << "\"";
                    }
                    else std::cout << "      {\"node\":" << n.inputs[j].nodeIndex << ", \"pin\":" << n.inputs[j].pinIndex << "}";
                }
                std::cout << "\n    ]";
            }
            if(n.HasOutFlow())
            {
                std::cout << ",\n    \"next\":";
                if(n.flowOutputIndex >= 0) std::cout << n.flowOutputIndex;
                else std::cout << "null";
            }
            std::cout << "\n  }";
        }
        std::cout << "\n]";
        break;
    }
}

void OnDisplay()
{
    editor.Draw();
    glutSwapBuffers();
}

struct Point { float x,y; };
struct Color { float r,g,b; };

void DrawLine(const Color & color, const Point & p0, const Point & p1)
{
    glBegin(GL_LINES);
    glColor3fv(&color.r);
    glVertex2fv(&p0.x);
    glVertex2fv(&p1.x);
    glEnd();
}

void DrawTriangle(const Color & color, const Point & p0, const Point & p1, const Point & p2)
{
    glBegin(GL_TRIANGLES);
    glColor3fv(&color.r);
    glVertex2fv(&p0.x);
    glVertex2fv(&p1.x);
    glVertex2fv(&p2.x);
    glEnd();
}

void DrawCircle(const Color & color, const Point & center, float radius)
{
    glBegin(GL_TRIANGLE_FAN);
    glColor3fv(&color.r);
    for(int i=0; i<24; ++i)
    {
        float a = i*6.28f/24;
        glVertex2f(center.x + std::cos(a)*radius, center.y + std::sin(a)*radius);
    }
    glEnd();
}

void OnSketchpadDisplay() {}

auto jsonSource = R"json(
[
  {
    "x":273,
    "y":155,
    "id":"event:Start",
    "next":1
  },
  {
    "x":504,
    "y":199,
    "id":"func:void DrawCircle(Color const &,Point const &,float)",
    "wires":[
      {"node":2, "pin":0},
      {"node":3, "pin":0},
      ".5"
    ],
    "next":4
  },
  {
    "x":250,
    "y":233,
    "id":"build:Color",
    "wires":[
      "1",
      ".5",
      "0"
    ]
  },
  {
    "x":147,
    "y":510,
    "id":"build:Point",
    "wires":[
      ".2",
      "0"
    ]
  },
  {
    "x":813,
    "y":245,
    "id":"func:void DrawTriangle(Color const &,Point const &,Point const &,Point const &)",
    "wires":[
      {"node":5, "pin":0},
      {"node":7, "pin":0},
      {"node":3, "pin":0},
      {"node":6, "pin":0}
    ],
    "next":null
  },
  {
    "x":528,
    "y":97,
    "id":"build:Color",
    "wires":[
      "0",
      ".5",
      "1"
    ]
  },
  {
    "x":379,
    "y":551,
    "id":"build:Point",
    "wires":[
      "-.6",
      "0"
    ]
  },
  {
    "x":484,
    "y":297,
    "id":"build:Point",
    "wires":[
      "-.2",
      ".8"
    ]
  }
]
)json";

int main(int argc, char * argv[])
{
    TypeLibrary types;
    types.BindFunction(&DrawLine, "DrawLine", {"color","p0","p1"});
    types.BindFunction(&DrawTriangle, "DrawTriangle", {"color","p0","p1","p2"});
    types.BindFunction(&DrawCircle, "DrawCircle", {"color","center","radius"});
    types.BindClass<Point>("Point")
        .HasField(&Point::x, "x")
        .HasField(&Point::y, "y");
    types.BindClass<Color>("Color")
        .HasField(&Color::r, "r")
        .HasField(&Color::g, "g")
        .HasField(&Color::b, "b");

    editor.nodeTypes.push_back(NodeType::MakeEventNode("Start"));
    for(auto & func : types.GetAllFunctions())
    {
        editor.nodeTypes.push_back(NodeType::MakeFunctionNode(func));
    }
    editor.nodeTypes.push_back(NodeType::MakeBuildNode(types.DeduceType<Point>()));
    editor.nodeTypes.push_back(NodeType::MakeSplitNode(types.DeduceType<Point>()));
    editor.nodeTypes.push_back(NodeType::MakeBuildNode(types.DeduceType<Color>()));
    editor.nodeTypes.push_back(NodeType::MakeSplitNode(types.DeduceType<Color>()));

    auto jGraph = jsonFrom(jsonSource);
    std::cout << tabbed(jGraph,4) << std::endl;

    // Create the stored nodes
    for(const auto & jNode : jGraph.array())
    {    
        const int2 position = { jNode["x"].number<int>(), jNode["y"].number<int>() };
        const auto & id = jNode["id"].string();

        NodeType * nodeType = nullptr;
        for(auto & type : editor.nodeTypes)
        {
            if(id == type.GetUniqueId())
            {
                nodeType = &type;
                break;
            }
        }

        if(!nodeType) throw std::runtime_error("Unrecognized node type: "+id);
        editor.nodes.push_back(Node(position, nodeType));

        auto & node = editor.nodes.back();
        const auto & jWires = jNode["wires"].array();
        if(jWires.size() != node.inputs.size()) throw std::runtime_error("Node input count mismatch: "+id);

        // Connect the wires
        for(size_t i=0; i<jWires.size(); ++i)
        {
            const auto & jWire = jWires[i];
            if(jWire.isString()) node.inputs[i] = {-1,-1,jWire.string()}; // Immediate value
            else if(jWire.isObject()) node.inputs[i] = {jWire["node"].number<int>(), jWire["pin"].number<int>()}; // Wired to other node
            else node.inputs[i] = {-1,-1}; // Not hooked up yet
        }

        // Connect flow wires
        node.flowOutputIndex = jNode["next"].numberOrDefault(-1);
    }
    assert(editor.nodes.size() == jGraph.array().size());
    // TODO: Validation

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);

    glutInitWindowSize(1280, 720);
    glutInitWindowPosition(80, 80);
    g_editorGlutWindow = glutCreateWindow("libmirror call graph visualizer");
    glutIdleFunc(OnIdle);
    glutDisplayFunc(OnDisplay);
    glutPassiveMotionFunc(OnMotion);
    glutMotionFunc(OnMotion);
    glutMouseFunc(OnMouse);
    glutKeyboardFunc(OnKeyboard);

    glutInitWindowSize(400, 400);
    glutInitWindowPosition(1440, 80);
    g_sketchpadGlutWindow = glutCreateWindow("sketchpad");
    glutDisplayFunc(OnSketchpadDisplay);

    glutMainLoop();

    return 0;
}