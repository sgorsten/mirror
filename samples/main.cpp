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
    if(editor.creatingNewNode)
    {

    }
    else
    {
        editor.mouseOverFeature = editor.GetFeature(x, y);

        if(editor.clickedFeature.type == Feature::Body)
        {
            for(auto & node : editor.nodes)
            {
                if(node.selected)
                {
                    node.position.x += x - editor.lastPos.x;
                    node.position.y += y - editor.lastPos.y;
                }
            }
        }

        editor.lastPos.x = x;
        editor.lastPos.y = y;
    }
}

void OnMouse(int button, int state, int x, int y)
{
    switch(button)
    {
    case GLUT_LEFT_BUTTON:
        if(state == GLUT_DOWN)
        {
            editor.clicked = true;
            editor.clickedPos.x = x;
            editor.clickedPos.y = y;

            if(editor.creatingNewNode)
            {
                int index = (y - editor.lastPos.y) / 16;
                if(index >= 0 && index < editor.nodeTypes.size())
                {
                    editor.nodes.push_back(Node(editor.lastPos.x, editor.lastPos.y, &editor.nodeTypes[index]));
                }
                editor.creatingNewNode = false;
            }
            else
            {
                editor.lastPos.x = x;
                editor.lastPos.y = y;
                editor.clickedFeature = editor.mouseOverFeature;
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
            }
        }
        else
        {
            editor.clicked = false;

            if(editor.clickedFeature.type == Feature::None)
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

            editor.ConnectPins(editor.clickedFeature, editor.mouseOverFeature);
            editor.clickedFeature = {};
        }
        break;
    case GLUT_RIGHT_BUTTON:
        if(state == GLUT_DOWN)
        {
            if(editor.mouseOverFeature.type == Feature::None)
            {
                editor.lastPos.x = x;
                editor.lastPos.y = y;
                editor.creatingNewNode = true;
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

struct Renderer
{
    void DrawText12(int x, int y, const std::string & text)
    {
        glRasterPos2i(x,y+12);
        for(auto ch : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, ch);
    }

    void DrawText18(int x, int y, const std::string & text)
    {
        glRasterPos2i(x,y+18);
        for(auto ch : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, ch);
    }

    void DrawRect(int x0, int y0, int x1, int y1)
    {
        glBegin(GL_QUADS);
        glVertex2i(x0,y0);
        glVertex2i(x1,y0);
        glVertex2i(x1,y1);
        glVertex2i(x0,y1);
        glEnd();
    }

    void DrawRect(const Rect & r)
    {
        DrawRect(r.x0, r.y0, r.x1, r.y1);
    }

    void DrawLine(const int2 & a, const int2 & b)
    {
        float x0 = a.x, y0 = a.y;
        float x3 = b.x, y3 = b.y;
        float x1 = (x0+x3)/2, y1 = y0;
        float x2 = (x0+x3)/2, y2 = y3;

        glBegin(GL_LINE_STRIP);
        for(float i=0; i<=32; ++i)
        {
            float t = i/32, s = (1-t);
            glVertex2f(x0*(s*s*s) + x1*(3*s*s*t) + x2*(3*s*t*t) + x3*(t*t*t), y0*(s*s*s) + y1*(3*s*s*t) + y2*(3*s*t*t) + y3*(t*t*t));
        }
        glEnd();
    }
};

void OnDisplay()
{
    Renderer r;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glOrtho(0, 1280, 720, 0, -1, 1);

    glColor3f(1,1,0);
    for(auto n : editor.nodes)
    {
        if(!n.selected) continue;
        auto rect = n.GetNodeRect();
        r.DrawRect(rect.x0-4, rect.y0-4, rect.x1+4, rect.y1+4);
    }

    // Draw node contents
    for(const auto & n : editor.nodes)
    {
        auto rect = n.GetNodeRect();
        glColor3f(0.3f,0.3f,0.3f);
        r.DrawRect(rect);

        rect = n.GetContentsRect();

        const auto & label = n.nodeType->GetLabel();
        if(!label.empty())
        {
            glColor3f(1,1,1);
            r.DrawText18(rect.x0, rect.y0, label);
            rect.y0 += n.GetLineSpacing();
        }

        if(n.IsSequenced())
        {
            rect = n.GetFlowInputRect();
            int cx = (rect.x0 + rect.x1) / 2;
            int cy = (rect.y0 + rect.y1) / 2;
            glBegin(GL_TRIANGLE_FAN);
            if(editor.clickedFeature.type == Feature::FlowOutput) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0, rect.y0);
            glVertex2i(cx, rect.y0);
            glVertex2i(rect.x1, cy);
            glVertex2i(cx, rect.y1);
            glVertex2i(rect.x0, rect.y1);
            glEnd();            

            rect = n.GetFlowOutputRect();
            cx = (rect.x0 + rect.x1) / 2;
            cy = (rect.y0 + rect.y1) / 2;
            glBegin(GL_TRIANGLE_FAN);
            if(editor.clickedFeature.type == Feature::FlowInput) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0, rect.y0);
            glVertex2i(cx, rect.y0);
            glVertex2i(rect.x1, cy);
            glVertex2i(cx, rect.y1);
            glVertex2i(rect.x0, rect.y1);
            glEnd();            
        }

        for(size_t i=0; i<n.GetInputCount(); ++i)
        {
            rect = n.GetInputPinRect(i);
            if(editor.clickedFeature.type == Feature::Output && editor.clickedFeature.GetPinType().type == n.GetInputType(i).type) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            r.DrawRect(rect);

            glColor3f(1,1,1);
            r.DrawText12(rect.x1 + n.GetPinPadding(), rect.y0, n.GetInputLabel(i));
            if(n.inputs[i].nodeIndex >= 0)
            {
                auto & n2 = editor.nodes[n.inputs[i].nodeIndex];
                auto pin = n.inputs[i].pinIndex;
                std::string val = ToStr(*n2.GetOutputType(pin).type, n2.outputValues[pin].get());
                r.DrawText12(rect.x0 - n.GetPinPadding() - GetStringWidth12(val), rect.y0, val);
            }
            else if(!n.inputs[i].immediate.empty())
            {
                std::string val = n.inputs[i].immediate;
                glColor3f(0,1,1);
                r.DrawText12(rect.x0 - n.GetPinPadding() - GetStringWidth12(val), rect.y0, val);
            }
        }

        for(size_t i=0; i<n.GetOutputCount(); ++i)
        {
            rect = n.GetOutputPinRect(i);
            if(editor.clickedFeature.type == Feature::Input && editor.clickedFeature.GetPinType().type == n.GetOutputType(i).type) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            r.DrawRect(rect);

            glColor3f(1,1,1);
            auto lbl = n.GetOutputLabel(i);
            r.DrawText12(rect.x0 - n.GetPinPadding() - GetStringWidth12(lbl), rect.y0, lbl);
            r.DrawText12(rect.x1 + n.GetPinPadding(), rect.y0, ToStr(*n.GetOutputType(i).type, n.outputValues[i].get()));
        }
    }

    // Draw wires
    glColor3f(1,1,1);
    for(const auto & n : editor.nodes)
    {
        if(n.IsSequenced() && n.flowOutputIndex >= 0)
        {
            r.DrawLine(n.GetFlowOutputRect().GetCenter(), editor.nodes[n.flowOutputIndex].GetFlowInputRect().GetCenter());
        }
        for(size_t i=0; i<n.inputs.size(); ++i)
        {
            auto wire = n.inputs[i];
            if(wire.nodeIndex >= 0 && wire.nodeIndex < editor.nodes.size())
            {
                r.DrawLine(editor.nodes[wire.nodeIndex].GetOutputPinRect(wire.pinIndex).GetCenter(), n.GetInputPinRect(i).GetCenter());
            }
        }
    }
    if(editor.clickedFeature.type == Feature::FlowInput)
    {
        if(editor.mouseOverFeature.type == Feature::FlowOutput) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);

        r.DrawLine(editor.clickedFeature.node->GetFlowInputRect().GetCenter(), editor.lastPos);
    }
    if(editor.clickedFeature.type == Feature::FlowOutput)
    {
        if(editor.mouseOverFeature.type == Feature::FlowInput) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);

        r.DrawLine(editor.clickedFeature.node->GetFlowOutputRect().GetCenter(), editor.lastPos);
    }
    if(editor.clickedFeature.type == Feature::Input)
    {
        if(editor.mouseOverFeature.type == Feature::Output && editor.mouseOverFeature.GetPinType().type == editor.clickedFeature.GetPinType().type) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);

        r.DrawLine(editor.clickedFeature.GetPinRect().GetCenter(), editor.lastPos);
    }
    if(editor.clickedFeature.type == Feature::Output)
    {
        if(editor.mouseOverFeature.type == Feature::Input && editor.clickedFeature.GetPinType().type == editor.mouseOverFeature.GetPinType().type) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);

        r.DrawLine(editor.clickedFeature.GetPinRect().GetCenter(), editor.lastPos);
    }

    std::string mouseOverText;
    if(editor.mouseOverFeature.IsPin())
    {
        std::string mouseOverText = ToString() << editor.mouseOverFeature.GetPinType();

        auto w = GetStringWidth12(mouseOverText);
        glBegin(GL_QUADS);
        glColor3f(0,0,0);
        r.DrawRect(editor.lastPos.x, editor.lastPos.y-16, editor.lastPos.x+w, editor.lastPos.y);
        glEnd();

        glColor3f(1,1,1);
        r.DrawText12(editor.lastPos.x, editor.lastPos.y-16, mouseOverText);
    }

    if(editor.creatingNewNode)
    {
        int cursor = editor.lastPos.y;
        for(const auto & type : editor.nodeTypes)
        {
            glColor3f(1,1,1);
            r.DrawText12(editor.lastPos.x, cursor, type.GetLabel());
            cursor += 16;
        }
    }
    else if(editor.clicked && editor.clickedFeature.type == Feature::None)
    {
        glBegin(GL_LINE_STRIP);
        glColor3f(0.5f,0.5f,0);
        r.DrawRect(editor.clickedPos.x, editor.clickedPos.y, editor.lastPos.x, editor.lastPos.y);
        glEnd();
    }

    glPopMatrix();   
    glPopAttrib();

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