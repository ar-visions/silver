package android.text;

import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.text.style.BackgroundColorSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.StrikethroughSpan;
import android.text.style.StyleSpan;
import android.text.style.SubscriptSpan;
import android.text.style.SuperscriptSpan;
import android.text.style.TypefaceSpan;
import android.text.style.URLSpan;
import android.text.style.UnderlineSpan;
import java.util.ArrayList;
import org.xml.sax.XMLReader;

public class Html {
	public static final int FROM_HTML_MODE_LEGACY = 0;
	public static final int FROM_HTML_SEPARATOR_LINE_BREAK_PARAGRAPH = 1;
	public static final int FROM_HTML_SEPARATOR_LINE_BREAK_HEADING = 2;
	public static final int FROM_HTML_SEPARATOR_LINE_BREAK_LIST_ITEM = 4;
	public static final int FROM_HTML_SEPARATOR_LINE_BREAK_LIST = 8;
	public static final int FROM_HTML_SEPARATOR_LINE_BREAK_DIV = 16;
	public static final int FROM_HTML_SEPARATOR_LINE_BREAK_BLOCKQUOTE = 32;
	public static final int FROM_HTML_MODE_COMPACT = 63;
	public static final int FROM_HTML_OPTION_USE_CSS_COLORS = 256;
	public static final int TO_HTML_PARAGRAPH_LINES_CONSECUTIVE = 0;
	public static final int TO_HTML_PARAGRAPH_LINES_INDIVIDUAL = 1;

	public static interface ImageGetter {
		public Drawable getDrawable(String source);
	}

	public static interface TagHandler {
		/**
		 * This method will be called whenn the HTML parser encounters
		 * a tag that it does not know how to interpret.
		 */
		public void handleTag(boolean opening, String tag, Editable output, XMLReader xmlReader);
	}

	public static Spanned fromHtml(String source) {
		return fromHtml(source, FROM_HTML_MODE_LEGACY, null, null);
	}

	public static Spanned fromHtml(String source, int flags) {
		return fromHtml(source, flags, null, null);
	}

	public static Spanned fromHtml(String source, ImageGetter imageGetter, TagHandler tagHandler) {
		return fromHtml(source, FROM_HTML_MODE_LEGACY, imageGetter, tagHandler);
	}

	public static Spanned fromHtml(String source, int flags, ImageGetter imageGetter, TagHandler tagHandler) {
		return new Converter(source == null ? "" : source, flags, tagHandler).convert();
	}

	public static String toHtml(Spanned text) {
		return escapeHtml(text);
	}

	public static String toHtml(Spanned text, int option) {
		return escapeHtml(text);
	}

	public static String escapeHtml(CharSequence source) {
		StringBuilder out = new StringBuilder(source.length());
		for (int i = 0; i < source.length(); i++) {
			char c = source.charAt(i);
			if (c == '<' || c == '>' || c == '&' || c == '"' || c == '\'' || c > 0x7F) {
				out.append("&#");
				out.append((int)c);
				out.append(';');
			} else {
				out.append(c);
			}
		}
		return out.toString();
	}

	// an open tag waiting for its end: the span starts at `at`
	private static class Open {
		String tag;
		int at;
		Object[] spans;
		Open(String tag, int at, Object[] spans) {
			this.tag = tag;
			this.at = at;
			this.spans = spans;
		}
	}

	// Android's HtmlToSpannedConverter, without TagSoup
	private static class Converter {
		private final String src;
		private final int flags;
		private final TagHandler handler;
		private final SpannableStringBuilder out = new SpannableStringBuilder();
		private final ArrayList<Open> open = new ArrayList<>();

		Converter(String src, int flags, TagHandler handler) {
			this.src = src;
			this.flags = flags;
			this.handler = handler;
		}

		Spanned convert() {
			int i = 0;
			int n = src.length();
			while (i < n) {
				char c = src.charAt(i);
				if (c == '<') {
					int end = tag_end(i);
					if (end < 0) {
						text(src.substring(i));
						break;
					}
					tag(src.substring(i + 1, end));
					i = end + 1;
				} else {
					int next = src.indexOf('<', i);
					if (next < 0)
						next = n;
					text(src.substring(i, next));
					i = next;
				}
			}
			while (!open.isEmpty())
				close(open.get(open.size() - 1).tag);
			return out;
		}

		private int tag_end(int from) {
			char quote = 0;
			for (int i = from + 1; i < src.length(); i++) {
				char c = src.charAt(i);
				if (quote != 0) {
					if (c == quote)
						quote = 0;
				} else if (c == '"' || c == '\'') {
					quote = c;
				} else if (c == '>') {
					return i;
				}
			}
			return -1;
		}

		private void text(String raw) {
			String s = entities(raw);
			StringBuilder sb = new StringBuilder();
			for (int i = 0; i < s.length(); i++) {
				char c = s.charAt(i);
				if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
					char pred;
					int len = sb.length();
					if (len == 0) {
						len = out.length();
						pred = len == 0 ? '\n' : out.charAt(len - 1);
					} else {
						pred = sb.charAt(len - 1);
					}
					if (pred != ' ' && pred != '\n')
						sb.append(' ');
				} else {
					sb.append(c);
				}
			}
			out.append(sb);
		}

		private void tag(String body) {
			if (body.startsWith("!") || body.startsWith("?"))
				return;
			boolean closing = body.startsWith("/");
			if (closing)
				body = body.substring(1);
			boolean self = body.endsWith("/");
			if (self)
				body = body.substring(0, body.length() - 1);
			body = body.trim();
			int sp = 0;
			while (sp < body.length() && !Character.isWhitespace(body.charAt(sp)))
				sp++;
			String name = body.substring(0, sp).toLowerCase();
			String attrs = body.substring(sp);
			if (name.isEmpty())
				return;
			if (closing)
				close(name);
			else {
				start(name, attrs);
				if (self)
					close(name);
			}
		}

		private int margin(int bit) {
			return (flags & bit) != 0 ? 1 : 2;
		}

		private void newlines(int min) {
			int len = out.length();
			if (len == 0)
				return;
			int have = 0;
			for (int i = len - 1; i >= 0 && have < min && out.charAt(i) == '\n'; i--)
				have++;
			for (; have < min; have++)
				out.append('\n');
		}

		private int block_margin(String tag) {
			switch (tag) {
				case "p":
					return margin(FROM_HTML_SEPARATOR_LINE_BREAK_PARAGRAPH);
				case "div":
					return margin(FROM_HTML_SEPARATOR_LINE_BREAK_DIV);
				case "ul":
					return margin(FROM_HTML_SEPARATOR_LINE_BREAK_LIST);
				case "li":
					return margin(FROM_HTML_SEPARATOR_LINE_BREAK_LIST_ITEM);
				case "blockquote":
					return margin(FROM_HTML_SEPARATOR_LINE_BREAK_BLOCKQUOTE);
				case "h1": case "h2": case "h3":
				case "h4": case "h5": case "h6":
					return margin(FROM_HTML_SEPARATOR_LINE_BREAK_HEADING);
			}
			return 0;
		}

		private void start(String tag, String attrs) {
			if (tag.equals("br")) {
				out.append('\n');
				return;
			}
			int m = block_margin(tag);
			if (m > 0)
				newlines(m);
			Object[] spans = spans_for(tag, attrs);
			if (spans == null) {
				if (m == 0 && handler != null)
					handler.handleTag(true, tag, out, null);
				spans = new Object[0];
			}
			open.add(new Open(tag, out.length(), spans));
		}

		private void close(String tag) {
			if (tag.equals("br"))
				return;
			for (int k = open.size() - 1; k >= 0; k--) {
				Open o = open.get(k);
				if (!o.tag.equals(tag))
					continue;
				open.remove(k);
				int end = out.length();
				if (end > o.at)
					for (Object span : o.spans)
						out.setSpan(span, o.at, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
				int m = block_margin(tag);
				if (m > 0)
					newlines(m);
				else if (o.spans.length == 0 && handler != null)
					handler.handleTag(false, tag, out, null);
				return;
			}
		}

		private Object[] spans_for(String tag, String attrs) {
			switch (tag) {
				case "b": case "strong":
					return new Object[] {new StyleSpan(Typeface.BOLD)};
				case "i": case "em": case "cite": case "dfn":
					return new Object[] {new StyleSpan(Typeface.ITALIC)};
				case "u": case "ins":
					return new Object[] {new UnderlineSpan()};
				case "s": case "strike": case "del":
					return new Object[] {new StrikethroughSpan()};
				case "big":
					return new Object[] {new RelativeSizeSpan(1.25f)};
				case "small":
					return new Object[] {new RelativeSizeSpan(0.8f)};
				case "tt":
					return new Object[] {new TypefaceSpan("monospace")};
				case "sup":
					return new Object[] {new SuperscriptSpan()};
				case "sub":
					return new Object[] {new SubscriptSpan()};
				case "a":
					String href = attr(attrs, "href");
					return href == null ? new Object[0] : new Object[] {new URLSpan(href)};
				case "h1": case "h2": case "h3":
				case "h4": case "h5": case "h6":
					float[] sizes = {1.5f, 1.4f, 1.3f, 1.2f, 1.1f, 1f};
					return new Object[] {new RelativeSizeSpan(sizes[tag.charAt(1) - '1']), new StyleSpan(Typeface.BOLD)};
				case "font":
					return color_spans(attr(attrs, "color"), null, false, attr(attrs, "face"));
				case "span":
					return style_spans(attr(attrs, "style"));
				case "p": case "div": case "ul": case "li": case "blockquote":
					Object[] s = style_spans(attr(attrs, "style"));
					return s;
				case "html": case "body": case "head": case "img":
					return new Object[0];
			}
			return null;
		}

		private Object[] style_spans(String style) {
			if (style == null)
				return new Object[0];
			String fg = null, bg = null;
			boolean strike = false;
			for (String decl : style.split(";")) {
				int colon = decl.indexOf(':');
				if (colon < 0)
					continue;
				String key = decl.substring(0, colon).trim().toLowerCase();
				String value = decl.substring(colon + 1).trim();
				if (key.equals("color"))
					fg = value;
				else if (key.equals("background-color") || key.equals("background"))
					bg = value;
				else if (key.equals("text-decoration") && value.contains("line-through"))
					strike = true;
			}
			return color_spans(fg, bg, strike, null);
		}

		private Object[] color_spans(String fg, String bg, boolean strike, String face) {
			ArrayList<Object> list = new ArrayList<>();
			int c;
			if (fg != null && (c = color(fg)) != 0)
				list.add(new ForegroundColorSpan(c));
			if (bg != null && (c = color(bg)) != 0)
				list.add(new BackgroundColorSpan(c));
			if (strike)
				list.add(new StrikethroughSpan());
			if (face != null)
				list.add(new TypefaceSpan(face));
			return list.toArray();
		}

		// 0 when unknown: no span
		private static int color(String s) {
			s = s.trim().toLowerCase();
			try {
				if (s.startsWith("#") && s.length() == 4) {
					s = "#" + s.charAt(1) + s.charAt(1) + s.charAt(2) + s.charAt(2) + s.charAt(3) + s.charAt(3);
				}
				return Color.parseColor(s) | 0xff000000;
			} catch (RuntimeException e) {
				return 0;
			}
		}

		private static String attr(String attrs, String name) {
			int i = 0;
			int n = attrs.length();
			while (i < n) {
				while (i < n && Character.isWhitespace(attrs.charAt(i)))
					i++;
				int k = i;
				while (i < n && attrs.charAt(i) != '=' && !Character.isWhitespace(attrs.charAt(i)))
					i++;
				String key = attrs.substring(k, i).toLowerCase();
				while (i < n && Character.isWhitespace(attrs.charAt(i)))
					i++;
				String value = "";
				if (i < n && attrs.charAt(i) == '=') {
					i++;
					while (i < n && Character.isWhitespace(attrs.charAt(i)))
						i++;
					if (i < n && (attrs.charAt(i) == '"' || attrs.charAt(i) == '\'')) {
						char q = attrs.charAt(i++);
						int e = attrs.indexOf(q, i);
						if (e < 0)
							e = n;
						value = attrs.substring(i, e);
						i = e + 1;
					} else {
						int v = i;
						while (i < n && !Character.isWhitespace(attrs.charAt(i)))
							i++;
						value = attrs.substring(v, i);
					}
				}
				if (key.equals(name))
					return entities(value);
				if (key.isEmpty())
					i++;
			}
			return null;
		}

		private static String entities(String s) {
			if (s.indexOf('&') < 0)
				return s;
			StringBuilder sb = new StringBuilder(s.length());
			int i = 0;
			while (i < s.length()) {
				char c = s.charAt(i);
				int semi = c == '&' ? s.indexOf(';', i) : -1;
				if (semi < 0 || semi - i > 10) {
					sb.append(c);
					i++;
					continue;
				}
				String name = s.substring(i + 1, semi);
				int code = -1;
				try {
					if (name.startsWith("#x") || name.startsWith("#X"))
						code = Integer.parseInt(name.substring(2), 16);
					else if (name.startsWith("#"))
						code = Integer.parseInt(name.substring(1));
				} catch (NumberFormatException e) {
				}
				if (code < 0) {
					switch (name) {
						case "amp": code = '&'; break;
						case "lt": code = '<'; break;
						case "gt": code = '>'; break;
						case "quot": code = '"'; break;
						case "apos": code = '\''; break;
						case "nbsp": code = 0xa0; break;
						case "copy": code = 0xa9; break;
						case "reg": code = 0xae; break;
						case "trade": code = 0x2122; break;
						case "hellip": code = 0x2026; break;
						case "mdash": code = 0x2014; break;
						case "ndash": code = 0x2013; break;
						case "laquo": code = 0xab; break;
						case "raquo": code = 0xbb; break;
					}
				}
				if (code < 0) {
					sb.append(c);
					i++;
					continue;
				}
				sb.appendCodePoint(code);
				i = semi + 1;
			}
			return sb.toString();
		}
	}
}
