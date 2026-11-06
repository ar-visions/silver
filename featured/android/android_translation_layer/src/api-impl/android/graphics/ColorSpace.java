package android.graphics;

public class ColorSpace {

	public static enum Named {
		SRGB,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named ACES;
	ACES,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named ACESCG;
	ACESCG,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named ADOBE_RGB;
	ADOBE_RGB,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named BT2020;
	BT2020,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named BT2020_HLG;
	BT2020_HLG,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named BT2020_PQ;
	BT2020_PQ,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named BT709;
	BT709,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named CIE_LAB;
	CIE_LAB,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named CIE_XYZ;
	CIE_XYZ,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named DCI_P3;
	DCI_P3,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named DISPLAY_P3;
	DISPLAY_P3,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named EXTENDED_SRGB;
	EXTENDED_SRGB,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named LINEAR_EXTENDED_SRGB;
	LINEAR_EXTENDED_SRGB,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named LINEAR_SRGB;
	LINEAR_SRGB,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named NTSC_1953;
	NTSC_1953,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named PRO_PHOTO_RGB;
	PRO_PHOTO_RGB,
	// GEN member:android.graphics.ColorSpace$Named:public static final android.graphics.ColorSpace$Named SMPTE_C;
	SMPTE_C
	}

	public static ColorSpace get(Named named) {
		return new ColorSpace();
	}

	// GEN class:android.graphics.ColorSpace$Rgb
	public class Rgb extends android.graphics.ColorSpace {
		// GEN member:android.graphics.ColorSpace$Rgb:public android.graphics.ColorSpace$Rgb(java.lang.String, float[], java.util.function.DoubleUnaryOperator, java.util.function.DoubleUnaryOperator);
		public Rgb(java.lang.String arg0, float[] arg1, java.util.function.DoubleUnaryOperator arg2, java.util.function.DoubleUnaryOperator arg3) {}
	
		// GEN member:android.graphics.ColorSpace$Rgb:public android.graphics.ColorSpace$Rgb(java.lang.String, float[], float[], java.util.function.DoubleUnaryOperator, java.util.function.DoubleUnaryOperator, float, float);
		public Rgb(java.lang.String arg0, float[] arg1, float[] arg2, java.util.function.DoubleUnaryOperator arg3, java.util.function.DoubleUnaryOperator arg4, float arg5, float arg6) {}
	
		// GEN member:android.graphics.ColorSpace$Rgb:public android.graphics.ColorSpace$Rgb(java.lang.String, float[], android.graphics.ColorSpace$Rgb$TransferParameters);
		public Rgb(java.lang.String arg0, float[] arg1, android.graphics.ColorSpace.Rgb.TransferParameters arg2) {}
	
		// GEN member:android.graphics.ColorSpace$Rgb:public android.graphics.ColorSpace$Rgb(java.lang.String, float[], float[], android.graphics.ColorSpace$Rgb$TransferParameters);
		public Rgb(java.lang.String arg0, float[] arg1, float[] arg2, android.graphics.ColorSpace.Rgb.TransferParameters arg3) {}
	
		// GEN member:android.graphics.ColorSpace$Rgb:public android.graphics.ColorSpace$Rgb(java.lang.String, float[], double);
		public Rgb(java.lang.String arg0, float[] arg1, double arg2) {}
	
		// GEN member:android.graphics.ColorSpace$Rgb:public android.graphics.ColorSpace$Rgb(java.lang.String, float[], float[], double);
		public Rgb(java.lang.String arg0, float[] arg1, float[] arg2, double arg3) {}
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] getWhitePoint(float[]);
		public float[] getWhitePoint(float[] arg0) { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] getWhitePoint();
		public float[] getWhitePoint() { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] getPrimaries(float[]);
		public float[] getPrimaries(float[] arg0) { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] getPrimaries();
		public float[] getPrimaries() { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] getTransform(float[]);
		public float[] getTransform(float[] arg0) { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] getTransform();
		public float[] getTransform() { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] getInverseTransform(float[]);
		public float[] getInverseTransform(float[] arg0) { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] getInverseTransform();
		public float[] getInverseTransform() { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public java.util.function.DoubleUnaryOperator getOetf();
		public java.util.function.DoubleUnaryOperator getOetf() { return null; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public java.util.function.DoubleUnaryOperator getEotf();
		public java.util.function.DoubleUnaryOperator getEotf() { return null; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public android.graphics.ColorSpace$Rgb$TransferParameters getTransferParameters();
		public android.graphics.ColorSpace.Rgb.TransferParameters getTransferParameters() { return null; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public boolean isSrgb();
		public boolean isSrgb() { return false; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public boolean isWideGamut();
		public boolean isWideGamut() { return false; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float getMinValue(int);
		public float getMinValue(int arg0) { return 0; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float getMaxValue(int);
		public float getMaxValue(int arg0) { return 0; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] toLinear(float, float, float);
		public float[] toLinear(float arg0, float arg1, float arg2) { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] toLinear(float[]);
		public float[] toLinear(float[] arg0) { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] fromLinear(float, float, float);
		public float[] fromLinear(float arg0, float arg1, float arg2) { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] fromLinear(float[]);
		public float[] fromLinear(float[] arg0) { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] toXyz(float[]);
		public float[] toXyz(float[] arg0) { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public float[] fromXyz(float[]);
		public float[] fromXyz(float[] arg0) { return new float[0]; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public boolean equals(java.lang.Object);
		public boolean equals(java.lang.Object arg0) { return false; }
	
		// GEN member:android.graphics.ColorSpace$Rgb:public int hashCode();
		public int hashCode() { return 0; }
	
		protected Rgb() {}
	
	// GEN class:android.graphics.ColorSpace$Rgb$TransferParameters
	public class TransferParameters {
		// GEN member:android.graphics.ColorSpace$Rgb$TransferParameters:public final double a;
		public double a;
	
		// GEN member:android.graphics.ColorSpace$Rgb$TransferParameters:public final double b;
		public double b;
	
		// GEN member:android.graphics.ColorSpace$Rgb$TransferParameters:public final double c;
		public double c;
	
		// GEN member:android.graphics.ColorSpace$Rgb$TransferParameters:public final double d;
		public double d;
	
		// GEN member:android.graphics.ColorSpace$Rgb$TransferParameters:public final double e;
		public double e;
	
		// GEN member:android.graphics.ColorSpace$Rgb$TransferParameters:public final double f;
		public double f;
	
		// GEN member:android.graphics.ColorSpace$Rgb$TransferParameters:public final double g;
		public double g;
	
		// GEN member:android.graphics.ColorSpace$Rgb$TransferParameters:public android.graphics.ColorSpace$Rgb$TransferParameters(double, double, double, double, double);
		public TransferParameters(double arg0, double arg1, double arg2, double arg3, double arg4) {}
	
		// GEN member:android.graphics.ColorSpace$Rgb$TransferParameters:public android.graphics.ColorSpace$Rgb$TransferParameters(double, double, double, double, double, double, double);
		public TransferParameters(double arg0, double arg1, double arg2, double arg3, double arg4, double arg5, double arg6) {}
	
		// GEN member:android.graphics.ColorSpace$Rgb$TransferParameters:public boolean equals(java.lang.Object);
		public boolean equals(java.lang.Object arg0) { return false; }
	
		// GEN member:android.graphics.ColorSpace$Rgb$TransferParameters:public int hashCode();
		public int hashCode() { return 0; }
	
		protected TransferParameters() {}
	}

}


	// GEN member:android.graphics.ColorSpace:public abstract boolean isWideGamut();
	public boolean isWideGamut() { return false; }

	// GEN member:android.graphics.ColorSpace:public java.lang.String getName();
	public java.lang.String getName() { return null; }

	// GEN member:android.graphics.ColorSpace:public boolean isSrgb();
	public boolean isSrgb() { return false; }
}
