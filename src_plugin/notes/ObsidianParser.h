#ifndef OBSIDIANPARSER_H
#define OBSIDIANPARSER_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariantMap>

struct ObsidianLink {
    QString raw;           // e.g. "[[Folder/Note#Heading|Alias]]" or "![[image.png|640x480]]"
    QString target;        // e.g. "Folder/Note" or "image.png"
    QString heading;       // e.g. "Heading" (empty if none)
    QString blockId;       // e.g. "block-id" (empty if none)
    QString alias;         // e.g. "Alias" (empty if none)
    bool isEmbed = false;  // true if ![[...]]
    bool isImage = false;  // true if target has image extension
    int imageWidth = 0;    // parsed image width (e.g. 640 or 100)
    int imageHeight = 0;   // parsed image height (e.g. 480)
    QString displayText;   // calculated display text
};

struct FrontmatterData {
    QString rawFrontmatter; // raw text including --- markers
    QString rawBody;        // raw markdown body after frontmatter
    bool hasFrontmatter = false;

    QVariantMap properties;
    QString color;
    bool isPinned = false;
    QStringList tags;
    QStringList aliases;
};

struct CalloutInfo {
    QString type;           // e.g. "note", "warning", "tip", "faq"
    QString title;          // e.g. "Important Notice"
    QString foldState;      // "+" (expanded), "-" (collapsed), "" (non-foldable)
    bool isFoldable = false;
    bool isCollapsed = false;
    QString body;           // content lines without '>' markers
    QString raw;            // complete raw text of callout block
};

class ObsidianParser {
public:
    // Link & Embed Parsing
    static ObsidianLink parseLink(const QString &rawLink);
    static QVector<ObsidianLink> extractAllLinks(const QString &text);
    static bool isImageExtension(const QString &path);

    // Frontmatter Splitting & Merging
    static FrontmatterData splitFrontmatter(const QString &fileContent);
    static QString mergeFrontmatter(const QString &originalContent,
                                   const QVariantMap &updatedFields,
                                   const QString &newBody = QString());

    // Callout Parsing
    static bool parseCallout(const QString &quoteText, CalloutInfo &info);
};

#endif // OBSIDIANPARSER_H
