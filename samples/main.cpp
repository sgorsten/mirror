#include "graph.h"

#include <GL/freeglut.h>

GraphEditor editor;

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
    int2 mousePos = {x,y}, delta = mousePos - editor.lastPos;
    editor.lastPos = mousePos;
    editor.mouseOverFeature = editor.GetFeature(x, y);

    switch(editor.mode)
    {
    case GraphEditor::NewNodePopup:
        editor.mouseOverFeature = {};
        break;
    case GraphEditor::Dragging:
        switch(editor.clickedFeature.type)
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
            editor.clickedPos = {x,y};
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
                editor.lastPos = editor.clickedPos;
                editor.clickedFeature = editor.mouseOverFeature;
                editor.mode = GraphEditor::Dragging;
                if(editor.clickedFeature.type == Feature::Input)
                {
                    editor.clickedFeature.node->inputs[editor.clickedFeature.pin] = {-1,-1};
                }

                if(editor.clickedFeature.type != Feature::Body && (glutGetModifiers() & GLUT_ACTIVE_SHIFT) == 0)
                {
                    for(auto & n : editor.nodes) n.selected = false;
                }

                if(editor.clickedFeature.type == Feature::Body)
                {
                    if(glutGetModifiers() & GLUT_ACTIVE_CTRL)
                    {
                        editor.clickedFeature.node->selected = !editor.clickedFeature.node->selected;
                    }
                    else
                    {
                        if(!editor.clickedFeature.node->selected)
                        {
                            for(auto & n : editor.nodes) n.selected = false;
                            editor.clickedFeature.node->selected = true;
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
                if(editor.clickedFeature.type == Feature::None) // Box selection
                {
                    Rect selectRect = { editor.clickedPos.x, editor.clickedPos.y, x, y };
                    if(selectRect.x0 > selectRect.x1) std::swap(selectRect.x0, selectRect.x1);
                    if(selectRect.y0 > selectRect.y1) std::swap(selectRect.y0, selectRect.y1);
      
                    for(auto & node : editor.nodes)
                    {
                        auto nodeRect = node.GetNodeRect();
                        if(nodeRect.x0 > selectRect.x1) continue;
                        if(nodeRect.x1 < selectRect.x0) continue;
                        if(nodeRect.y0 > selectRect.y1) continue;
                        if(nodeRect.y1 < selectRect.y0) continue;
                        node.selected = true;
                    }
                }
                else editor.ConnectPins(editor.clickedFeature, editor.mouseOverFeature); // Wire connection
                break;
            }
            editor.mode = GraphEditor::None;
            editor.clickedFeature = {};
        }
        break;
    case GLUT_RIGHT_BUTTON:
        if(state == GLUT_DOWN)
        {
            if(editor.mouseOverFeature.type == Feature::None)
            {
                editor.menuPos = {x,y};
                editor.mode = GraphEditor::NewNodePopup;
            }
            else
            {
                editor.RecomputeNode(*editor.mouseOverFeature.node);
            }
        }
        break;
    }
}

#include <iostream>

void OnKeyboard(unsigned char key, int x, int y)
{
    if(editor.clickedFeature.type == Feature::Input)
    {
        auto & im = editor.clickedFeature.node->inputs[editor.clickedFeature.pin].immediate;
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
            if(n.IsSequenced())
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

int neg(int a) { return -a; }
int add(int a, int b) { return a+b; }
int mul(int a, int b) { return a*b; }
float negf(float a) { return -a; }
float addf(float a, float b) { return a+b; }
float mulf(float a, float b) { return a*b; }

struct Character
{
    float x;
    float y;
    int hp;

    Character() : x(), y(), hp(100) {}
    Character(int hp) : x(), y(), hp(hp) {}

    void move(float dx, float dy) { x+=dx; y+=dy; }
    void damage(int dmg) { hp -= dmg; }
    void heal(int healing) { hp += healing; }
};

int main(int argc, char * argv[])
{
    TypeLibrary types;
    types.BindFunction(&neg,  "neg", {});
    types.BindFunction(&add,  "add", {"a","b"});
    types.BindFunction(&mul,  "mul", {"a","b"});
    types.BindFunction(&negf, "negf", {});
    types.BindFunction(&addf, "addf", {"a","b"});
    types.BindFunction(&mulf, "mulf", {"a","b"});
    types.BindClass<Character>("Character")
        .HasField(&Character::x,  "x")
        .HasField(&Character::y,  "y")
        .HasField(&Character::hp, "hp")
        .HasMethod(&Character::move,   "move",   {"dx","dy"})
        .HasMethod(&Character::damage, "damage", {"dmg"})
        .HasMethod(&Character::heal,   "heal",   {"hp"})
        .HasConstructor<int>({"hp"});

    for(auto & func : types.GetAllFunctions())
    {
        editor.nodeTypes.push_back(NodeType::MakeFunctionNode(func));
    }
    editor.nodeTypes.push_back(NodeType::MakeBuildNode(types.DeduceType<Character>()));
    editor.nodeTypes.push_back(NodeType::MakeSplitNode(types.DeduceType<Character>()));

    glutInit(&argc, argv);
    glutInitWindowSize(1280, 720);
    glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);
    glutCreateWindow("libmirror call graph visualizer");

    glutIdleFunc(OnIdle);
    glutDisplayFunc(OnDisplay);
    //glutReshapeFunc(OnReshape);
    glutPassiveMotionFunc(OnMotion);
    glutMotionFunc(OnMotion);
    glutMouseFunc(OnMouse);
    glutKeyboardFunc(OnKeyboard);
    glutMainLoop();

    return 0;
}