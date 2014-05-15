#include "graph.h"

#include <GL/freeglut.h>

GraphEditor editor;

size_t GetStringWidth(const std::string & text)
{
    size_t w = 0;
    for(auto ch : text) w += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, ch);
    return w;
}

void OnIdle() 
{ 
    glutPostRedisplay(); 
}

void OnMotion(int x, int y)
{
    editor.SetMousePosition(x, y);

    if(editor.clickedFeature.type == Feature::Body)
    {
        editor.clickedFeature.node->x += x - editor.lastX;
        editor.clickedFeature.node->y += y - editor.lastY;
    }

    editor.lastX = x;
    editor.lastY = y;
}

void OnMouse(int button, int state, int x, int y)
{
    switch(button)
    {
    case GLUT_LEFT_BUTTON:
        if(state == GLUT_DOWN)
        {
            editor.lastX = x;
            editor.lastY = y;
            editor.clickedFeature = editor.mouseOverFeature;
            if(editor.clickedFeature.type == Feature::Input)
            {
                editor.clickedFeature.node->inputs[editor.clickedFeature.pin] = -1;
            }
        }
        else
        {
            editor.ConnectPins(editor.clickedFeature, editor.mouseOverFeature);
            editor.clickedFeature = {};
        }
        break;
    case GLUT_RIGHT_BUTTON:
        if(editor.mouseOverFeature.type != Feature::None)
        {
            editor.RecomputeNode(*editor.mouseOverFeature.node);
        }
        break;
    }
}

void RenderText(int x, int y, const std::string & text)
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
        if(n.function)
        {
            glColor3f(1,1,1);
            RenderText(rect.x0, rect.y0, ToString() << n.function->GetName());
            rect.y0 += n.GetPinSpacing();
        }
       
        glColor3f(0.7f,0.7f,0.7f);
        RenderText(rect.x0, rect.y0, ToStr(*n.type.type, n.value.get()));

        for(size_t i=0; i<n.GetInputCount(); ++i)
        {
            rect = n.GetInputPinRect(i);
            glBegin(GL_QUADS);
            glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0,rect.y0);
            glVertex2i(rect.x1,rect.y0);
            glVertex2i(rect.x1,rect.y1);
            glVertex2i(rect.x0,rect.y1);
            glEnd();

            glColor3f(1,1,1);
            RenderText(rect.x1 + n.GetPinPadding(), rect.y0, ToString() << n.function->GetParamTypes()[i]);
        }

        for(size_t i=0; i<n.GetOutputCount(); ++i)
        {
            rect = n.GetOutputPinRect(i);
            glBegin(GL_QUADS);
            glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0,rect.y0);
            glVertex2i(rect.x1,rect.y0);
            glVertex2i(rect.x1,rect.y1);
            glVertex2i(rect.x0,rect.y1);
            glEnd();

            std::string s = ToString() << n.type;
            glColor3f(1,1,1);
            RenderText(rect.x0 - n.GetPinPadding() - GetStringWidth(s), rect.y0, s);
        }
    }

    // Draw wires
    glBegin(GL_LINES);
    glColor3f(1,1,1);
    for(const auto & n : editor.nodes)
    {
        for(size_t i=0; i<n.inputs.size(); ++i)
        {
            if(n.inputs[i] >= 0 && n.inputs[i] < editor.nodes.size())
            {
                auto a = editor.nodes[n.inputs[i]].GetOutputPinRect(0);
                auto b = n.GetInputPinRect(i);
                glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
                glVertex2f((b.x0+b.x1)*0.5f, (b.y0+b.y1)*0.5f);
            }
        }
    }
    if(editor.clickedFeature.type == Feature::Input)
    {
        auto a = editor.clickedFeature.node->GetInputPinRect(editor.clickedFeature.pin);
        glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
        glVertex2i(editor.lastX, editor.lastY);
    }
    if(editor.clickedFeature.type == Feature::Output)
    {
        auto a = editor.clickedFeature.node->GetOutputPinRect(editor.clickedFeature.pin);
        glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
        glVertex2i(editor.lastX, editor.lastY);
    }
    glEnd();

    glPopMatrix();   
    glPopAttrib();

    glutSwapBuffers();
}

int add(int a, int b) { return a+b; }
int mul(int a, int b) { return a*b; }

int main(int argc, char * argv[])
{
    TypeLibrary types;
    types.BindFunction(&add,"add");
    types.BindFunction(&mul,"mul");

    editor.nodes.resize(5);

    editor.nodes[0] = Node(100, 100, types, 2);
    editor.nodes[1] = Node(100, 300, types, 3);

    editor.nodes[2] = Node(500, 200, types.GetFunction("add"));
    editor.nodes[2].inputs = {0,1};

    editor.nodes[3] = Node(500, 400, types, 8);

    editor.nodes[4] = Node(900, 300, types.GetFunction("mul"));
    editor.nodes[4].inputs = {2,3};

    // Evaluate call graph
    for(auto & n : editor.nodes) editor.RecomputeNode(n);

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
    //glutKeyboardFunc(OnKeyboard);
    glutMainLoop();

    return 0;
}