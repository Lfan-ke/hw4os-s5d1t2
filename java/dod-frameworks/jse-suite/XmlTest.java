import javax.xml.parsers.*;
import javax.xml.xpath.*;
import org.w3c.dom.*;
import org.xml.sax.*;
import org.xml.sax.helpers.DefaultHandler;
import java.io.*;

/* DoD: javax.xml (java.xml 模块) — DOM 解析 + XPath 查询 + SAX 解析。纯 stdlib 自校验。 */
public class XmlTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }

    static final String XML = "<root><item id=\"1\">a</item><item id=\"2\">b</item><item id=\"3\">c</item></root>";

    public static void main(String[] args) throws Exception {
        // DOM parse
        DocumentBuilder db = DocumentBuilderFactory.newInstance().newDocumentBuilder();
        Document doc = db.parse(new InputSource(new StringReader(XML)));
        NodeList items = doc.getElementsByTagName("item");
        check(items.getLength() == 3, "dom-elements");
        check(items.item(0).getTextContent().equals("a"), "dom-textcontent");
        check(((Element) items.item(1)).getAttribute("id").equals("2"), "dom-attribute");

        // XPath
        XPath xp = XPathFactory.newInstance().newXPath();
        String v = xp.evaluate("/root/item[@id='3']/text()", doc);
        check(v.equals("c"), "xpath-predicate");
        Double cnt = (Double) xp.evaluate("count(/root/item)", doc, XPathConstants.NUMBER);
        check(cnt.intValue() == 3, "xpath-count");

        // SAX
        int[] saxCount = {0};
        SAXParserFactory.newInstance().newSAXParser().parse(
            new InputSource(new StringReader(XML)),
            new DefaultHandler() {
                public void startElement(String u, String l, String q, Attributes a) {
                    if (q.equals("item")) saxCount[0]++;
                }
            });
        check(saxCount[0] == 3, "sax-startelement");

        System.out.println("XML_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("XML_DONE");
    }
}
