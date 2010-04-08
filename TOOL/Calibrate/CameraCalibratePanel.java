
/****************************
 * Simple Panel to tweak camera calibration values
 * @author Octavian Neamtu
 ***************************/

package TOOL.Calibrate;

import TOOL.Calibrate.CalibrateModule;
import javax.swing.*;
import java.awt.*;
import java.text.NumberFormat;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
//TODO fix commenting and formatting on this
class CameraCalibratePanel extends JFrame implements PropertyChangeListener, ActionListener{

    private float[] calibrateArray;
    private JFormattedTextField[] valueFields;
    private JLabel[] labels;
    private int calibrateArraySize = 9;
    private NumberFormat textFieldFormat;
    private CalibrateModule calibrateModule;
    
    private enum CameraCalibrateItem{
        CAMERA_ROLL_ANGLE(0, "Camera Roll"),
        CAMERA_PITCH_ANGLE(1, "Camera Pitch"),
        CAMERA_PAN_ANGLE(2, "Camera Pan"),
        CAMERA_OFF_X(3, "Camera Off X"),
        CAMERA_OFF_Y(4, "Camera Off Y"),
        CAMERA_OFF_Z(5, "Camera Off Z"),
        HEAD_PAN(6, "Head Pan"),
        HEAD_PITCH(7, "Head Pitch"),
        NECK_OFFSET_Z(8, "Neck Off X");
    
        public final int index;
        public final String id;
    
        CameraCalibrateItem(int index, String id){
        this.index = index; this.id = id;
        }
    }

    public CameraCalibratePanel( CalibrateModule calibrateModule){
    super("Camera Calibrate");

    this.calibrateModule = calibrateModule;
    
    setLayout(new GridLayout(2,1));
    JPanel topPanel = new JPanel();
    topPanel.setLayout (new GridLayout(9,2));

    
    valueFields = new JFormattedTextField[calibrateArraySize];
    labels = new JLabel[calibrateArraySize];
    
    for (CameraCalibrateItem i: CameraCalibrateItem.values()){
           labels[i.index] = new JLabel(i.id);
           topPanel.add(labels[i.index]);
           valueFields[i.index] = new JFormattedTextField(textFieldFormat);
           topPanel.add(valueFields[i.index]);
           //valueFields[i.index].setValue(new Float(calibrateArray[i.index]));
           valueFields[i.index].addPropertyChangeListener("value", this);
           //System.out.println(i.id);
        }
    
    add(topPanel);
    
    JPanel bottomPanel = new JPanel();
    
    JButton getButton = new JButton("getData");
    getButton.setActionCommand("get");
    getButton.addActionListener(this);
    
    bottomPanel.add(getButton);
    
    JButton setButton = new JButton("setData");
    setButton.setActionCommand("set");
    setButton.addActionListener(this);
    
    bottomPanel.add(setButton);
    
    add(bottomPanel);
    pack();
    setVisible(true);
    }
    
    public void propertyChange(PropertyChangeEvent e){
        for (CameraCalibrateItem i: CameraCalibrateItem.values()){
            if (e.getSource().equals(valueFields[i.index]))
                if (valueFields[i.index].getValue() != null)
                    calibrateArray[i.index] = ((Number)valueFields[i.index].getValue()).floatValue();
        }
    }
    
    public void actionPerformed(ActionEvent e){
    if (e.getActionCommand().equals("get")){
        calibrateArray = calibrateModule.getCalibrate().getVisionState().getThreshImage().getVisionLink().getCameraCalibrate();
        for (CameraCalibrateItem i: CameraCalibrateItem.values()){
            valueFields[i.index].setValue(new Float(calibrateArray[i.index]));
        }
    }
    if (e.getActionCommand().equals("set")){
        calibrateModule.getCalibrate().getVisionState().getThreshImage().getVisionLink().setCameraCalibrate(calibrateArray);
    }
    
    }

};