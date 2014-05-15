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
    editor.mouseOverFeature = editor.GetFeature(x, y);

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
        if(state == GLUT_DOWN && editor.mouseOverFeature.type != Feature::None)
        {
            editor.RecomputeNode(*editor.mouseOverFeature.node);
        }
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
            RenderText18(rect.x0, rect.y0, ToString() << n.function->GetName());
            rect.y0 += n.GetLineSpacing();
        }
       
        glColor3f(0.7f,0.7f,0.7f);
        RenderText18(rect.x0, rect.y0, ToStr(*n.type.type, n.value.get()));

        for(size_t i=0; i<n.GetInputCount(); ++i)
        {
            rect = n.GetInputPinRect(i);
            glBegin(GL_QUADS);
            if(editor.clickedFeature.type == Feature::Output && editor.clickedFeature.node->type.type == n.function->GetParamTypes()[i].type) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0,rect.y0);
            glVertex2i(rect.x1,rect.y0);
            glVertex2i(rect.x1,rect.y1);
            glVertex2i(rect.x0,rect.y1);
            glEnd();

            glColor3f(1,1,1);
            RenderText12(rect.x1 + n.GetPinPadding(), rect.y0, ToString() << n.function->GetParamTypes()[i]);
        }

        for(size_t i=0; i<n.GetOutputCount(); ++i)
        {
            rect = n.GetOutputPinRect(i);
            glBegin(GL_QUADS);
            if(editor.clickedFeature.type == Feature::Input && editor.clickedFeature.node->function->GetParamTypes()[editor.clickedFeature.pin].type == n.type.type) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0,rect.y0);
            glVertex2i(rect.x1,rect.y0);
            glVertex2i(rect.x1,rect.y1);
            glVertex2i(rect.x0,rect.y1);
            glEnd();

            std::string s = ToString() << n.type;
            glColor3f(1,1,1);
            RenderText12(rect.x0 - n.GetPinPadding() - GetStringWidth12(s), rect.y0, s);
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
        if(editor.mouseOverFeature.type == Feature::Output && editor.mouseOverFeature.node->type.type == editor.clickedFeature.node->function->GetParamTypes()[editor.clickedFeature.pin].type) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);

        auto a = editor.clickedFeature.node->GetInputPinRect(editor.clickedFeature.pin);
        glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
        glVertex2i(editor.lastX, editor.lastY);
    }
    if(editor.clickedFeature.type == Feature::Output)
    {
        if(editor.mouseOverFeature.type == Feature::Input && editor.clickedFeature.node->type.type == editor.mouseOverFeature.node->function->GetParamTypes()[editor.mouseOverFeature.pin].type) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);

        auto a = editor.clickedFeature.node->GetOutputPinRect(editor.clickedFeature.pin);
        glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
        glVertex2i(editor.lastX, editor.lastY);
    }
    glEnd();

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

    float getX() const { return x; }
    float getY() const { return y; }
    int getHp() const { return hp; }

    void move(float dx, float dy) { x+=dx; y+=dy; }
    void damage(int dmg) { hp -= dmg; }
    void heal(int healing) { hp += healing; }
};

int main(int argc, char * argv[])
{
    TypeLibrary types;
    types.BindFunction(&neg,"neg");
    types.BindFunction(&add,"add");
    types.BindFunction(&mul,"mul");
    types.BindFunction(&negf,"negf");
    types.BindFunction(&addf,"addf");
    types.BindFunction(&mulf,"mulf");
    types.BindFunction(&Character::getX,"getX");
    types.BindFunction(&Character::getY,"getY");
    types.BindFunction(&Character::getHp,"getHp");
    types.BindFunction(&Character::move,"move");
    types.BindFunction(&Character::damage,"damage");
    types.BindFunction(&Character::heal,"heal");

    editor.nodes.push_back(Node(100, 100, types, Character()));
    editor.nodes.push_back(Node(100, 200, types, 2));
    editor.nodes.push_back(Node(100, 300, types, 3));
    editor.nodes.push_back(Node(100, 400, types, 4.1f));
    editor.nodes.push_back(Node(100, 500, types, 7.2f));

    editor.nodes.push_back(Node(500, 100, types.GetFunction("neg")));
    editor.nodes.push_back(Node(500, 200, types.GetFunction("add")));
    editor.nodes.push_back(Node(500, 300, types.GetFunction("mul")));
    editor.nodes.push_back(Node(500, 400, types.GetFunction("negf")));
    editor.nodes.push_back(Node(500, 500, types.GetFunction("addf")));
    editor.nodes.push_back(Node(500, 600, types.GetFunction("mulf")));

    editor.nodes.push_back(Node(900, 100, types.GetFunction("getX")));
    editor.nodes.push_back(Node(900, 200, types.GetFunction("getY")));
    editor.nodes.push_back(Node(900, 300, types.GetFunction("getHp")));
    editor.nodes.push_back(Node(900, 400, types.GetFunction("move")));
    editor.nodes.push_back(Node(900, 500, types.GetFunction("damage")));
    editor.nodes.push_back(Node(900, 600, types.GetFunction("heal")));

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