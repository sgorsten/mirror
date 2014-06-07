#include "editor.h"

#include <GL/glut.h>

struct Renderer
{
    void DrawText12(const int2 & coord, const std::string & text)
    {
        glRasterPos2i(coord.x, coord.y+12);
        for(auto ch : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, ch);
    }

    void DrawText18(const int2 & coord, const std::string & text)
    {
        glRasterPos2i(coord.x, coord.y+18);
        for(auto ch : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, ch);
    }

    void DrawWireBox(int x0, int y0, int x1, int y1)
    {
        glBegin(GL_LINE_STRIP);
        glVertex2i(x0,y0);
        glVertex2i(x1,y0);
        glVertex2i(x1,y1);
        glVertex2i(x0,y1);
        glVertex2i(x0,y0);
        glEnd();
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

    void DrawRect(const Rect & r) { DrawRect(r.b0.x, r.b0.y, r.b1.x, r.b1.y); }

    void DrawLine(const int2 & a, const int2 & b)
    {
        auto p0 = float2(a.x, a.y);
        auto p3 = float2(b.x, b.y);
        auto p1 = float2((p0.x+p3.x)/2, p0.y);
        auto p2 = float2((p0.x+p3.x)/2, p3.y);
        auto d01 = p1-p0, d12 = p2-p1, d23 = p3-p2;

        glBegin(GL_TRIANGLE_STRIP);
        for(float i=0; i<=32; ++i)
        {
            float t = i/32, s = (1-t);
            auto p = p0*(s*s*s) + p1*(3*s*s*t) + p2*(3*s*t*t) + p3*(t*t*t);
            auto d = norm(d01*(3*s*s) + d12*(6*s*t) + d23*(3*t*t));
            glVertex2f(p.x-d.y, p.y+d.x);
            glVertex2f(p.x+d.y, p.y-d.x);
        }
        glEnd();
    }
};

void GraphEditor::Draw() const
{
    Renderer r;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glOrtho(0, 1280, 720, 0, -1, 1);

    // Draw outlines for selected nodes
    glColor3f(1,1,0);
    for(const auto & n : nodes)
    {
        if(!n.selected) continue;
        auto rect = n.GetNodeRect();
        r.DrawRect(rect.b0.x-4, rect.b0.y-4, rect.b1.x+4, rect.b1.y+4);
    }

    // Draw node backgrounds
    glColor3f(0.3f,0.3f,0.3f);
    for(const auto & n : nodes)
    {
        r.DrawRect(n.GetNodeRect());
    }

    // Draw wires
    glColor3f(1,1,1);
    for(const auto & n : nodes)
    {
        if(n.HasOutFlow() && n.flowOutputIndex >= 0)
        {
            r.DrawLine(n.GetFlowOutputRect().GetCenter(), nodes[n.flowOutputIndex].GetFlowInputRect().GetCenter());
        }
        for(size_t i=0; i<n.inputs.size(); ++i)
        {
            auto wire = n.inputs[i];
            if(wire.nodeIndex >= 0 && wire.nodeIndex < nodes.size())
            {
                r.DrawLine(nodes[wire.nodeIndex].GetOutputPinRect(wire.pinIndex).GetCenter(), n.GetInputPinRect(i).GetCenter());
            }
        }
    }
    if(clicked.IsDataPin() || clicked.IsFlowPin())
    {
        if(IsCompatible(clicked, mouseover)) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);
        r.DrawLine(clicked.GetPinRect().GetCenter(), mouseover.coord);
    }

    // Draw node contents
    for(const auto & n : nodes)
    {
        auto rect = n.GetContentsRect();
        const auto & label = n.nodeType->GetLabel();
        if(!label.empty())
        {
            glColor3f(1,1,1);
            r.DrawText18(rect.b0, label);
            rect.b0.y += n.GetLineSpacing();
        }

        if(n.HasInFlow())
        {
            rect = n.GetFlowInputRect();
            auto center = rect.GetCenter();
            glBegin(GL_TRIANGLE_FAN);
            if(clicked.type == Feature::FlowOutput) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.b0.x, rect.b0.y);
            glVertex2i(center.x, rect.b0.y);
            glVertex2i(rect.b1.x, center.y);
            glVertex2i(center.x, rect.b1.y);
            glVertex2i(rect.b0.x, rect.b1.y);
            glEnd();
        }          

        if(n.HasOutFlow())
        {
            rect = n.GetFlowOutputRect();
            auto center = rect.GetCenter();
            glBegin(GL_TRIANGLE_FAN);
            if(clicked.type == Feature::FlowInput) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.b0.x, rect.b0.y);
            glVertex2i(center.x, rect.b0.y);
            glVertex2i(rect.b1.x, center.y);
            glVertex2i(center.x, rect.b1.y);
            glVertex2i(rect.b0.x, rect.b1.y);
            glEnd();            
        }

        for(size_t i=0; i<n.GetInputCount(); ++i)
        {
            rect = n.GetInputPinRect(i);
            if(clicked.type == Feature::Output && IsAssignable(clicked.GetPinType(), n.GetInputType(i))) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            r.DrawRect(rect);

            glColor3f(1,1,1);
            r.DrawText12({rect.b1.x + n.GetPinPadding(), rect.b0.y}, n.GetInputLabel(i));
            if(n.inputs[i].nodeIndex < 0 && !n.inputs[i].immediate.empty())
            {
                std::string val = n.inputs[i].immediate;
                glColor3f(0,1,1);
                r.DrawText12(rect.b0 - int2(n.GetPinPadding() + GetStringWidth12(val), 0), val);
            }
        }

        for(size_t i=0; i<n.GetOutputCount(); ++i)
        {
            rect = n.GetOutputPinRect(i);
            if(clicked.type == Feature::Input && IsAssignable(n.GetOutputType(i), clicked.GetPinType())) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            r.DrawRect(rect);

            glColor3f(1,1,1);
            auto lbl = n.GetOutputLabel(i);
            r.DrawText12(rect.b0 - int2(n.GetPinPadding() + GetStringWidth12(lbl), 0), lbl);
        }
    }

    std::string mouseOverText;
    if(mouseover.IsDataPin())
    {
        std::string mouseOverText = ToString() << mouseover.GetPinType();

        auto w = GetStringWidth12(mouseOverText);
        glColor3f(0,0,0);
        r.DrawRect(mouseover.coord.x, mouseover.coord.y-16, mouseover.coord.x+w, mouseover.coord.y);
        glEnd();

        glColor3f(1,1,1);
        r.DrawText12(mouseover.coord - int2(0,16), mouseOverText);
    }

    switch(mode)
    {
    case Dragging:
        switch(clicked.type)
        {
        case Feature::None: // Box selection
            glColor3f(0.5f,0.5f,0);
            r.DrawWireBox(clicked.coord.x, clicked.coord.y, mouseover.coord.x, mouseover.coord.y);
            glEnd();
            break;
        }
        break;
    case NewNodePopup:
        {
            auto cursor = menuPos;
            for(const auto & type : nodeTypes)
            {
                glColor3f(1,1,1);
                r.DrawText12(cursor, type.GetLabel());
                cursor.y += 16;
            }
        }
        break;
    }

    glPopMatrix();   
    glPopAttrib();
}