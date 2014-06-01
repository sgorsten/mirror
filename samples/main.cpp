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
                    node.x += x - editor.lastX;
                    node.y += y - editor.lastY;
                }
            }
        }

        editor.lastX = x;
        editor.lastY = y;
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
            editor.clickedX = x;
            editor.clickedY = y;

            if(editor.creatingNewNode)
            {
                int index = (y - editor.lastY) / 16;
                if(index >= 0 && index < editor.nodeTypes.size())
                {
                    editor.nodes.push_back(Node(editor.lastX, editor.lastY, &editor.nodeTypes[index]));
                }
                editor.creatingNewNode = false;
            }
            else
            {
                editor.lastX = x;
                editor.lastY = y;
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
                Rect selectRect = { editor.clickedX, editor.clickedY, x, y };
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
                editor.lastX = x;
                editor.lastY = y;
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
            std::cout << "\n    \"x\":" << n.x;
            std::cout << ",\n    \"y\":" << n.y;
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
                if(n.flowOutput) std::cout << (n.flowOutput - editor.nodes.data());
                else std::cout << "null";
            }
            std::cout << "\n  }";
        }
        std::cout << "\n]";
        break;
    }
}

void RenderText12(int x, int y, const std::string & text)
{
    glRasterPos2i(x,y+12);
    for(auto ch : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, ch);
}

void RenderText18(int x, int y, const std::string & text)
{
    glRasterPos2i(x,y+18);
    for(auto ch : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, ch);
}

void OnDisplay()
{  
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glOrtho(0, 1280, 720, 0, -1, 1);

    glBegin(GL_QUADS);
    glColor3f(1,1,0);
    for(auto n : editor.nodes)
    {
        if(!n.selected) continue;
        auto rect = n.GetNodeRect();
        glVertex2i(rect.x0-4,rect.y0-4);
        glVertex2i(rect.x1+4,rect.y0-4);
        glVertex2i(rect.x1+4,rect.y1+4);
        glVertex2i(rect.x0-4,rect.y1+4);
    }
    glEnd();

    // Draw node contents
    for(const auto & n : editor.nodes)
    {
        auto rect = n.GetNodeRect();
        glBegin(GL_QUADS);
        glColor3f(0.3f,0.3f,0.3f);
        glVertex2i(rect.x0,rect.y0);
        glVertex2i(rect.x1,rect.y0);
        glVertex2i(rect.x1,rect.y1);
        glVertex2i(rect.x0,rect.y1);
        glEnd();

        rect = n.GetContentsRect();

        const auto & label = n.nodeType->GetLabel();
        if(!label.empty())
        {
            glColor3f(1,1,1);
            RenderText18(rect.x0, rect.y0, label);
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
            glBegin(GL_QUADS);
            if(editor.clickedFeature.type == Feature::Output && editor.clickedFeature.GetPinType().type == n.GetInputType(i).type) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0,rect.y0);
            glVertex2i(rect.x1,rect.y0);
            glVertex2i(rect.x1,rect.y1);
            glVertex2i(rect.x0,rect.y1);
            glEnd();

            glColor3f(1,1,1);
            RenderText12(rect.x1 + n.GetPinPadding(), rect.y0, n.GetInputLabel(i));
            if(n.inputs[i].nodeIndex >= 0)
            {
                auto & n2 = editor.nodes[n.inputs[i].nodeIndex];
                auto pin = n.inputs[i].pinIndex;
                std::string val = ToStr(*n2.GetOutputType(pin).type, n2.outputValues[pin].get());
                RenderText12(rect.x0 - n.GetPinPadding() - GetStringWidth12(val), rect.y0, val);
            }
            else if(!n.inputs[i].immediate.empty())
            {
                std::string val = n.inputs[i].immediate;
                glColor3f(0,1,1);
                RenderText12(rect.x0 - n.GetPinPadding() - GetStringWidth12(val), rect.y0, val);
            }
        }

        for(size_t i=0; i<n.GetOutputCount(); ++i)
        {
            rect = n.GetOutputPinRect(i);
            glBegin(GL_QUADS);
            if(editor.clickedFeature.type == Feature::Input && editor.clickedFeature.GetPinType().type == n.GetOutputType(i).type) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0,rect.y0);
            glVertex2i(rect.x1,rect.y0);
            glVertex2i(rect.x1,rect.y1);
            glVertex2i(rect.x0,rect.y1);
            glEnd();

            glColor3f(1,1,1);
            auto lbl = n.GetOutputLabel(i);
            RenderText12(rect.x0 - n.GetPinPadding() - GetStringWidth12(lbl), rect.y0, lbl);
            RenderText12(rect.x1 + n.GetPinPadding(), rect.y0, ToStr(*n.GetOutputType(i).type, n.outputValues[i].get()));
        }
    }

    // Draw wires
    glBegin(GL_LINES);
    glColor3f(1,1,1);
    for(const auto & n : editor.nodes)
    {
        if(n.IsSequenced() && n.flowOutput)
        {
            auto a = n.GetFlowOutputRect();
            auto b = n.flowOutput->GetFlowInputRect();
            glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
            glVertex2f((b.x0+b.x1)*0.5f, (b.y0+b.y1)*0.5f);            
        }
        for(size_t i=0; i<n.inputs.size(); ++i)
        {
            auto wire = n.inputs[i];
            if(wire.nodeIndex >= 0 && wire.nodeIndex < editor.nodes.size())
            {
                auto a = editor.nodes[wire.nodeIndex].GetOutputPinRect(wire.pinIndex);
                auto b = n.GetInputPinRect(i);
                glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
                glVertex2f((b.x0+b.x1)*0.5f, (b.y0+b.y1)*0.5f);
            }
        }
    }
    if(editor.clickedFeature.type == Feature::FlowInput)
    {
        if(editor.mouseOverFeature.type == Feature::FlowOutput) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);

        auto a = editor.clickedFeature.node->GetFlowInputRect();
        glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
        glVertex2i(editor.lastX, editor.lastY);
    }
    if(editor.clickedFeature.type == Feature::FlowOutput)
    {
        if(editor.mouseOverFeature.type == Feature::FlowInput) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);

        auto a = editor.clickedFeature.node->GetFlowOutputRect();
        glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
        glVertex2i(editor.lastX, editor.lastY);
    }
    if(editor.clickedFeature.type == Feature::Input)
    {
        if(editor.mouseOverFeature.type == Feature::Output && editor.mouseOverFeature.GetPinType().type == editor.clickedFeature.GetPinType().type) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);

        auto a = editor.clickedFeature.node->GetInputPinRect(editor.clickedFeature.pin);
        glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
        glVertex2i(editor.lastX, editor.lastY);
    }
    if(editor.clickedFeature.type == Feature::Output)
    {
        if(editor.mouseOverFeature.type == Feature::Input && editor.clickedFeature.GetPinType().type == editor.mouseOverFeature.GetPinType().type) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);

        auto a = editor.clickedFeature.node->GetOutputPinRect(editor.clickedFeature.pin);
        glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
        glVertex2i(editor.lastX, editor.lastY);
    }
    glEnd();

    std::string mouseOverText;
    if(editor.mouseOverFeature.IsPin())
    {
        std::string mouseOverText = ToString() << editor.mouseOverFeature.GetPinType();

        auto w = GetStringWidth12(mouseOverText);
        glBegin(GL_QUADS);
        glColor3f(0,0,0);
        glVertex2i(editor.lastX, editor.lastY);
        glVertex2i(editor.lastX, editor.lastY-16);
        glVertex2i(editor.lastX+w, editor.lastY-16);
        glVertex2i(editor.lastX+w, editor.lastY);
        glEnd();

        glColor3f(1,1,1);
        RenderText12(editor.lastX, editor.lastY-16, mouseOverText);
    }

    if(editor.creatingNewNode)
    {
        int cursor = editor.lastY;
        for(const auto & type : editor.nodeTypes)
        {
            glColor3f(1,1,1);
            RenderText12(editor.lastX, cursor, type.GetLabel());
            cursor += 16;
        }
    }
    else if(editor.clicked && editor.clickedFeature.type == Feature::None)
    {
        glBegin(GL_LINE_STRIP);
        glColor3f(0.5f,0.5f,0);
        glVertex2i(editor.clickedX, editor.clickedY);
        glVertex2i(editor.lastX, editor.clickedY);
        glVertex2i(editor.lastX, editor.lastY);
        glVertex2i(editor.clickedX, editor.lastY);
        glVertex2i(editor.clickedX, editor.clickedY);
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