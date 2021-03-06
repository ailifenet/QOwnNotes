#include "tag.h"
#include "notefolder.h"
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QSettings>


Tag::Tag() {
    id = 0;
    name = "";
    priority = 0;
    parentId = 0;
    _color = QColor();
}

int Tag::getId() {
    return this->id;
}

int Tag::getParentId() {
    return this->parentId;
}

void Tag::setParentId(int id) {
    this->parentId = id;
}

QString Tag::getName() {
    return this->name;
}

void Tag::setName(QString text) {
    this->name = text;
}

QColor Tag::getColor() {
    return this->_color;
}

void Tag::setColor(QColor color) {
    this->_color = color;
}

int Tag::getPriority() {
    return this->priority;
}

void Tag::setPriority(int value) {
    this->priority = value;
}

Tag Tag::fetch(int id) {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    Tag tag;

    query.prepare("SELECT * FROM tag WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else if (query.first()) {
        tag.fillFromQuery(query);
    }

    return tag;
}

Tag Tag::fetchByName(QString name) {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    Tag tag;

    query.prepare("SELECT * FROM tag WHERE LOWER(name) = :name");
    query.bindValue(":name", name.toLower());

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else if (query.first()) {
        tag.fillFromQuery(query);
    }

    return tag;
}

Tag Tag::fetchByName(QString name, int parentId) {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    Tag tag;

    query.prepare("SELECT * FROM tag WHERE LOWER(name) = :name AND"
                          "parent_id = :parent_id");
    query.bindValue(":name", name.toLower());
    query.bindValue(":parent_id", parentId);

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else if (query.first()) {
        tag.fillFromQuery(query);
    }

    return tag;
}

int Tag::countAll() {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    query.prepare("SELECT COUNT(*) AS cnt FROM tag");

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else if (query.first()) {
        return query.value("cnt").toInt();
    }

    return 0;
}

/**
 * Removes the tag, their children and its note link items
 */
bool Tag::remove() {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    // remove the tag
    query.prepare("DELETE FROM tag WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
        return false;
    } else {
        // remove all children tags
        Q_FOREACH(Tag tag, fetchAllByParentId(id)) {
                tag.remove();
            }

        // remove the note tag links
        query.prepare("DELETE FROM noteTagLink WHERE tag_id = :id");
        query.bindValue(":id", id);

        if (!query.exec()) {
            qWarning() << __func__ << ": " << query.lastError();
            return false;
        } else {
            return true;
        }
    }
}

Tag Tag::tagFromQuery(QSqlQuery query) {
    Tag tag;
    tag.fillFromQuery(query);
    return tag;
}

bool Tag::fillFromQuery(QSqlQuery query) {
    this->id = query.value("id").toInt();
    this->name = query.value("name").toString();
    this->priority = query.value("priority").toInt();
    this->parentId = query.value("parent_id").toInt();

    QString colorName = query.value("color").toString();
    this->_color = colorName.isEmpty() ? QColor() : QColor(colorName);

    return true;
}

QList<Tag> Tag::fetchAll() {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    QList<Tag> tagList;

    query.prepare("SELECT * FROM tag ORDER BY priority ASC, name ASC");
    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else {
        for (int r = 0; query.next(); r++) {
            Tag tag = tagFromQuery(query);
            tagList.append(tag);
        }
    }

    return tagList;
}

QList<Tag> Tag::fetchAllByParentId(int parentId) {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);
    QList<Tag> tagList;

    query.prepare("SELECT * FROM tag WHERE parent_id = :parentId ORDER BY "
                          "priority ASC, name ASC");
    query.bindValue(":parentId", parentId);

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else {
        for (int r = 0; query.next(); r++) {
            Tag tag = tagFromQuery(query);
            tagList.append(tag);
        }
    }

    return tagList;
}

int Tag::countAllParentId(int parentId) {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    query.prepare("SELECT COUNT(*) AS cnt FROM tag "
                          "WHERE parent_id = :parentId ");
    query.bindValue(":parentId", parentId);

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else if (query.first()) {
        return query.value("cnt").toInt();
    }

    return 0;
}

/**
 * Checks if the current tag has a child with tagId
 */
bool Tag::hasChild(int tagId) {
    Q_FOREACH(Tag tag, fetchAllByParentId(id)) {
            qDebug() << __func__ << " - 'tag': " << tag;

            if ((tag.getId() == tagId) || tag.hasChild(tagId)) {
                return true;
            }
        }

    return false;
}

/**
 * Fetches all linked tags of a note
 */
QList<Tag> Tag::fetchAllOfNote(Note note) {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    QList<Tag> tagList;

    query.prepare("SELECT t.* FROM tag t "
                          "JOIN noteTagLink l ON t.id = l.tag_id "
                          "WHERE l.note_file_name = :fileName AND "
                          "l.note_sub_folder_path = :noteSubFolderPath "
                          "ORDER BY t.priority ASC, t.name ASC");
    query.bindValue(":fileName", note.getName());
    query.bindValue(":noteSubFolderPath",
                    note.getNoteSubFolder().relativePath());

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else {
        for (int r = 0; query.next(); r++) {
            Tag tag = tagFromQuery(query);
            tagList.append(tag);
        }
    }

    return tagList;
}

/**
 * Fetches one Tag of a note that has a color
 */
Tag Tag::fetchOneOfNoteWithColor(Note note) {
    Q_FOREACH(Tag tag, fetchAllOfNote(note)) {
            if (tag.getColor().isValid()) {
                return tag;
            }
        }

    return Tag();
}

/**
 * Count all linked tags of a note
 */
int Tag::countAllOfNote(Note note) {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    query.prepare("SELECT COUNT(*) AS cnt FROM noteTagLink "
                          "WHERE note_file_name = :fileName AND "
                          "note_sub_folder_path = :noteSubFolderPath");
    query.bindValue(":fileName", note.getName());
    query.bindValue(":noteSubFolderPath",
                    note.getNoteSubFolder().relativePath());

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else if (query.first()) {
        return query.value("cnt").toInt();
    }

    return 0;
}

/**
 * Checks if tag is linked to a note
 */
bool Tag::isLinkedToNote(Note note) {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    query.prepare("SELECT COUNT(*) AS cnt FROM noteTagLink "
                          "WHERE note_file_name = :fileName AND "
                          "note_sub_folder_path = :noteSubFolderPath "
                          "AND tag_id = :tagId");
    query.bindValue(":fileName", note.getName());
    query.bindValue(":noteSubFolderPath",
                    note.getNoteSubFolder().relativePath());
    query.bindValue(":tagId", id);

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else if (query.first()) {
        return query.value("cnt").toInt() > 0;
    }

    return false;
}

/**
 * Returns all tags that are linked to certain note names
 */
QList<Tag> Tag::fetchAllWithLinkToNoteNames(QStringList noteNameList) {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);
    QList<Tag> tagList;
    QString noteIdListString = noteNameList.join("','");

    QString sql = QString(
            "SELECT t.* FROM tag t "
                "JOIN noteTagLink l ON t.id = l.tag_id "
                "WHERE l.note_file_name IN ('%1') AND "
                    "l.note_sub_folder_path = :noteSubFolderPath "
                "GROUP BY t.id "
                "ORDER BY t.priority ASC, t.name ASC")
            .arg(noteIdListString);
    query.prepare(sql);
    query.bindValue(":noteSubFolderPath",
                    NoteSubFolder::activeNoteSubFolder().relativePath());

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else {
        for (int r = 0; query.next(); r++) {
            Tag tag = tagFromQuery(query);
            tagList.append(tag);
        }
    }

    return tagList;
}

/**
 * Fetches all linked note file names
 */
QStringList Tag::fetchAllLinkedNoteFileNames() {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);
    QStringList fileNameList;

    query.prepare("SELECT note_file_name FROM noteTagLink WHERE tag_id = :id "
                          "AND note_sub_folder_path = :noteSubFolderPath");
    query.bindValue(":id", this->id);
    query.bindValue(":noteSubFolderPath",
                    NoteSubFolder::activeNoteSubFolder().relativePath());

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else {
        for (int r = 0; query.next(); r++) {
            fileNameList.append(query.value("note_file_name").toString());
        }
    }

    return fileNameList;
}

/**
 * Converts backslashes to slashes in the noteTagLink table to fix
 * problems with Windows
 */
void Tag::convertDirSeparator() {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    query.prepare("UPDATE noteTagLink SET note_sub_folder_path = replace("
                          "note_sub_folder_path, '\\', '/')");

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    }
}

/**
 * Fetches all tag names
 */
QStringList Tag::fetchAllNames() {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    QStringList nameList;

    query.prepare("SELECT name FROM tag ORDER BY name");

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else {
        for (int r = 0; query.next(); r++) {
            nameList.append(query.value("name").toString());
        }
    }

    return nameList;
}

/**
 * Count the linked note file names
 */
int Tag::countLinkedNoteFileNames() {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    query.prepare("SELECT COUNT(note_file_name) AS cnt FROM noteTagLink "
                          "WHERE tag_id = :id AND "
                          "note_sub_folder_path = :noteSubFolderPath");
    query.bindValue(":id", this->id);
    query.bindValue(":noteSubFolderPath",
                    NoteSubFolder::activeNoteSubFolder().relativePath());

    if (!query.exec()) {
        qWarning() << __func__ << ": " << query.lastError();
    } else if (query.first()) {
        return query.value("cnt").toInt();
    }

    return 0;
}

/**
 * Inserts or updates a Tag object in the database
 */
bool Tag::store() {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);

    if (this->id > 0) {
        query.prepare(
                "UPDATE tag SET name = :name, priority = :priority, "
                        "parent_id = :parentId, color = :color "
                        "WHERE id = :id");
        query.bindValue(":id", this->id);
    } else {
        query.prepare(
                "INSERT INTO tag (name, priority, parent_id, color) "
                        "VALUES (:name, :priority, :parentId, :color)");
    }

    query.bindValue(":name", this->name);
    query.bindValue(":priority", this->priority);
    query.bindValue(":parentId", this->parentId);
    query.bindValue(":color", _color.isValid() ? _color.name() : "");

    if (!query.exec()) {
        // on error
        qWarning() << __func__ << ": " << query.lastError();
        return false;
    } else if (this->id == 0) {
        // on insert
        this->id = query.lastInsertId().toInt();
    }

    return true;
}

/**
 * Links a note to a tag
 */
bool Tag::linkToNote(Note note) {
    if (!isFetched()) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);
    query.prepare("INSERT INTO noteTagLink (tag_id, note_file_name, "
                          "note_sub_folder_path) "
                          "VALUES (:tagId, :noteFileName, "
                          ":noteSubFolderPath)");

    query.bindValue(":tagId", this->id);
    query.bindValue(":noteFileName", note.getName());
    query.bindValue(":noteSubFolderPath",
                    note.getNoteSubFolder().relativePath());

    if (!query.exec()) {
        // on error
        qWarning() << __func__ << ": " << query.lastError();
        return false;
    }

    return true;
}

/**
 * Removes the link to a note
 */
bool Tag::removeLinkToNote(Note note) {
    if (!isFetched()) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);
    query.prepare("DELETE FROM noteTagLink WHERE tag_id = :tagId AND "
                          "note_file_name = :noteFileName AND "
                          "note_sub_folder_path = :noteSubFolderPath");

    query.bindValue(":tagId", this->id);
    query.bindValue(":noteFileName", note.getName());
    query.bindValue(":noteSubFolderPath",
                    note.getNoteSubFolder().relativePath());

    if (!query.exec()) {
        // on error
        qWarning() << __func__ << ": " << query.lastError();
        return false;
    }

    return true;
}

/**
 * Removes all links to a note
 */
bool Tag::removeAllLinksToNote(Note note) {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);
    query.prepare("DELETE FROM noteTagLink WHERE "
                          "note_file_name = :noteFileName AND "
                          "note_sub_folder_path = :noteSubFolderPath");

    query.bindValue(":noteFileName", note.getName());
    query.bindValue(":noteSubFolderPath",
                    note.getNoteSubFolder().relativePath());

    if (!query.exec()) {
        // on error
        qWarning() << __func__ << ": " << query.lastError();
        return false;
    }

    return true;
}

/**
 * Renames the note file name of note links
 */
bool Tag::renameNoteFileNamesOfLinks(QString oldFileName, QString newFileName) {
    QSqlDatabase db = QSqlDatabase::database("note_folder");
    QSqlQuery query(db);
    query.prepare("UPDATE noteTagLink SET note_file_name = :newFileName WHERE "
                          "note_file_name = :oldFileName AND "
                          "note_sub_folder_path = :noteSubFolderPath");

    query.bindValue(":oldFileName", oldFileName);
    query.bindValue(":newFileName", newFileName);
    query.bindValue(":noteSubFolderPath",
                    NoteSubFolder::activeNoteSubFolder().relativePath());

    if (!query.exec()) {
        // on error
        qWarning() << __func__ << ": " << query.lastError();
        return false;
    }

    return true;
}

/**
 * Checks if the active tag still exists in the database
 */
bool Tag::exists() {
    Tag tag = Tag::fetch(this->id);
    return tag.id > 0;
}

bool Tag::isFetched() {
    return (this->id > 0);
}

void Tag::setAsActive() {
    Tag::setAsActive(id);
}

void Tag::setAsActive(int tagId) {
    NoteFolder noteFolder = NoteFolder::currentNoteFolder();
    noteFolder.setActiveTagId(tagId);
    noteFolder.store();
}

/**
 * Checks if this note folder is the active one
 */
bool Tag::isActive() {
    return activeTagId() == id;
}

/**
 * Returns the id of the active note folder in the settings
 */
int Tag::activeTagId() {
    NoteFolder noteFolder = NoteFolder::currentNoteFolder();
    return noteFolder.getActiveTagId();
}

/**
 * Returns the active note folder
 */
Tag Tag::activeTag() {
    return Tag::fetch(activeTagId());
}

QDebug operator<<(QDebug dbg, const Tag &tag) {
    dbg.nospace() << "Tag: <id>" << tag.id << " <name>" << tag.name <<
            " <parentId>" << tag.parentId;
    return dbg.space();
}
