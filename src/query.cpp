/*
 * Copyright (C) 2016 Roman Shchekin aka mrqtros.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <query.h>
#include <localization.h>

#include <unity/scopes/Annotation.h>
#include <unity/scopes/CategorisedResult.h>
#include <unity/scopes/CategoryRenderer.h>
#include <unity/scopes/QueryBase.h>
#include <unity/scopes/SearchReply.h>

#include <iomanip>
#include <iostream>
#include <sstream>

Q_LOGGING_CATEGORY(Qry, "Query")

namespace sc = unity::scopes;

using namespace std;


const static string CURRENT_TEMPLATE_OV =
        R"(
{
        "schema-version": 1,
        "template": {
        "category-layout": "grid",
        "card-size": "medium",
        "overlay": true
        },
        "components": {
        "title": "title",
        "art" : {
        "field": "art"
        }
        }
        }
        )";


const static string CURRENT_TEMPLATE =
        R"(
{
        "schema-version": 1,
        "template": {
        "category-layout": "grid",
        "card-size": "medium"
        },
        "components": {
        "title": "title",
        "art" : {
        "field": "art"
        }
        }
        }
        )";

Query::Query(const sc::CannedQuery &query, const sc::SearchMetadata &metadata, Config::Ptr config) :
    sc::SearchQueryBase(query, metadata),
    //client_(config),
    m_config(config),
    m_coursera(config),
    m_udemy(config),
    m_edx(config),
    m_udacity(config),
    m_iversity(config),
    m_openLearning(config)
{

}

void Query::cancelled()
{
    qCDebug(Qry) << "Cancelled";
    //client_.cancel();
    m_coursera.cancel();
    m_udemy.cancel();
    m_edx.cancel();
    m_udacity.cancel();
    m_iversity.cancel();
    m_openLearning.cancel();
}


void Query::run(sc::SearchReplyProxy const& reply)
{
    try {
        // Start by getting information about the query
        const sc::CannedQuery &query(sc::SearchQueryBase::query());

        // Get the query string
        string query_string = query.query_string();
        qCDebug(Qry) << "Query string is:" << QString::fromStdString(query_string);

        // Checking out which sources are enabled.
        QList<BaseClient*> sources;
        if (settings().at("coursera").get_bool())
            sources.append(&m_coursera);
        if (settings().at("udemy").get_bool())
            sources.append(&m_udemy);
        if (settings().at("edx").get_bool())
            sources.append(&m_edx);
        if (settings().at("udacity").get_bool())
            sources.append(&m_udacity);
        if (settings().at("iversity").get_bool())
            sources.append(&m_iversity);
        if (settings().at("openLearning").get_bool())
            sources.append(&m_openLearning);
        qCDebug(Qry) << "Source count:" << sources.count();

        // Working with departments.
        sc::Department::SPtr all_depts = sc::Department::create("", query, _("All"));
        sc::DepartmentList depList;
        QList<Department> list = DepartmentManager::departments();
        for (int i = 0; i < list.length(); i++)
            depList.push_back(sc::Department::create(list[i].id.toStdString(), query, list[i].label.toStdString()));
        all_depts->set_subdepartments(depList);

        // Register the root department on the reply
        reply->register_departments(all_depts);

        QString selectedDepartment = QString::fromStdString(query.department_id());
        qCDebug(Qry) << "Selected department:" << selectedDepartment;

        QSet<QString> uniqueSet;
        for(int i = 0; i < sources.count(); ++i)
        {
            QString sourceCatName = sources[i]->name();
            auto sourceList = sources[i]->courses(QString::fromStdString(query_string));

            auto templ = settings().at("textPosition").get_int() ? CURRENT_TEMPLATE_OV : CURRENT_TEMPLATE;
            auto sourceCategory = reply->register_category(sourceCatName.toLower().toStdString(),
                                                             sourceCatName.toStdString(),
                                                             "", sc::CategoryRenderer(templ));

            //qCDebug(Qry) << "Processing source:" << sourceCatName;

            for (const auto &course : sourceList)
            {

                // Department check.
                if (!selectedDepartment.isEmpty() && !DepartmentManager::isMatch(course, selectedDepartment))
                    continue;

                // Duplicate results cause Dash to crash.
                if (uniqueSet.contains(course.link))
                {
                    qCDebug(Qry) << "Duplicate:" << course.link;
                    continue;
                }
                uniqueSet.insert(course.link);


                sc::CategorisedResult res(sourceCategory);

                // We must have a URI
                res.set_uri(course.link.toStdString());
                res.set_title(course.title.toStdString());
                res.set_art(course.art.toStdString());
                res["headline"] = course.headline.toStdString();
                res["description"] = course.description.toStdString();
                res["source"] = sourceCatName.toStdString();
                res["extra"] = course.extra.toStdString();
                if (!course.video.isEmpty())
                    res["video_url"] = course.video.toStdString();
                res["departments"] = course.departments.join(';').toStdString();

                // Add instructors to map.
                if (course.instructors.count() > 0)
                {
                    std::vector<sc::Variant> images, names, bios;
                    for (auto j = course.instructors.begin(); j != course.instructors.end(); ++j)
                    {
                        images.push_back(sc::Variant(j->image.toStdString()));
                        names.push_back(sc::Variant(j->name.toStdString()));
                        bios.push_back(sc::Variant((j->bio.length() < 150 ? j->bio : j->bio.left(150) + "...").toStdString()));
                    }
                    res["instr_images"] = images;
                    res["instr_names"] = names;
                    res["instr_bios"] = bios;
                }

                // Push the result
                if (!reply->push(res))
                    return;
            }

            qCDebug(Qry) << "Finished with source:" << sourceCatName;
        }

    } catch (domain_error &e) {
        // Handle exceptions being thrown by the client API
        cerr << e.what() << endl;
        reply->error(current_exception());
    }
}

