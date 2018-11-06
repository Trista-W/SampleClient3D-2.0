using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

using NatNetML;

namespace WindowsFormsApp1
{
    public partial class Form1 : Form
    {

        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            // read image
            Bitmap bmp = new Bitmap("D:\\Git\\LoadImage\\Texture.bmp");

            // load image in picturebox
            pictureBox1.Image = bmp;

        }

        private void pictureBox1_Paint(object sender, PaintEventArgs e)
        {
            Brush brush = new SolidBrush(Color.Red);
            e.Graphics.FillEllipse(brush, 150, 150, 10, 10);
        }
    }
}




