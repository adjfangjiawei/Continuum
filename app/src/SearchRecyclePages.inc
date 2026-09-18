

class GlobalSearchPage final : public wxPanel
{
public:
    explicit GlobalSearchPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY),
          completionTimer_(this)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* advanced = Button(this, U("高级筛选"));
        auto* save = Button(this, U("保存查询"));
        searchButton_ = Button(this, U("搜索"), true);

        actions->Add(advanced, 0, wxRIGHT, 10);
        actions->Add(save, 0, wxRIGHT, 10);
        actions->Add(searchButton_, 0);

        AddHeading(
            this,
            root,
            U("P07"),
            U("全局搜索"),
            U("跨文档、证据和业务对象执行非阻塞调查式查询"),
            actions
        );

        auto* queryCard = Card(this);
        auto* queryLayout = new wxBoxSizer(wxHORIZONTAL);

        query_ = Input(queryCard, wxTE_PROCESS_ENTER);
        query_->SetName("global.search.query");
        query_->SetHint(
            U("输入关键字，或使用 type:decision、status:active、owner:姓名")
        );
        query_->SetMinSize(wxSize(-1, 40));

        queryLayout->Add(query_, 1, wxEXPAND | wxALL, 16);
        queryCard->SetSizer(queryLayout);

        root->Add(
            queryCard,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            26
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* filters = Card(this);
        auto* filterLayout = new wxBoxSizer(wxVERTICAL);

        filterLayout->Add(
            Label(
                filters,
                U("结果筛选"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        type_ = new wxChoice(filters, wxID_ANY);
        type_->Append(
            U("全部类型"),
            new wxStringClientData("")
        );
        type_->Append(
            U("业务对象"),
            new wxStringClientData("object")
        );
        type_->Append(
            U("文档"),
            new wxStringClientData("file")
        );
        type_->Append(
            U("证据"),
            new wxStringClientData("evidence")
        );
        type_->SetSelection(0);

        statusFilter_ = new wxChoice(filters, wxID_ANY);
        statusFilter_->Append(
            U("全部状态"),
            new wxStringClientData("")
        );
        statusFilter_->Append(
            U("有效 / 已接受"),
            new wxStringClientData("active")
        );
        statusFilter_->Append(
            U("待确认 / 待复核"),
            new wxStringClientData("review")
        );
        statusFilter_->Append(
            U("已关闭 / 已拒绝"),
            new wxStringClientData("closed")
        );
        statusFilter_->SetSelection(0);

        AddField(filterLayout, filters, U("类型"), type_);
        AddField(
            filterLayout,
            filters,
            U("状态"),
            statusFilter_
        );

        filterLayout->Add(
            Label(
                filters,
                U(
                    "支持的条件：\n"
                    "type:decision\n"
                    "status:active\n"
                    "owner:姓名\n\n"
                    "条件值始终作为参数绑定，"
                    "不会作为 SQL 执行。"
                ),
                9,
                Theme::Muted()
            ),
            0,
            wxALL,
            16
        );

        filterLayout->AddStretchSpacer();
        filters->SetSizer(filterLayout);
        filters->SetMinSize(wxSize(235, -1));

        auto* resultsCard = Card(this);
        auto* resultsLayout = new wxBoxSizer(wxVERTICAL);
        auto* resultsHeader = new wxBoxSizer(wxHORIZONTAL);

        resultsHeader->Add(
            Label(
                resultsCard,
                U("搜索结果"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );

        resultStatus_ = Label(
            resultsCard,
            U("输入查询后开始搜索"),
            9,
            Theme::Muted()
        );

        resultsHeader->Add(
            resultStatus_,
            0,
            wxALIGN_CENTER_VERTICAL
        );

        resultsLayout->Add(
            resultsHeader,
            0,
            wxEXPAND | wxALL,
            16
        );

        results_ = new wxListCtrl(
            resultsCard,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxLC_REPORT |
                wxLC_SINGLE_SEL |
                wxBORDER_NONE
        );

        results_->SetBackgroundColour(Theme::Input());
        results_->SetForegroundColour(Theme::Text());
        results_->SetFont(Theme::Font(9));
        results_->InsertColumn(
            0,
            U("编号"),
            wxLIST_FORMAT_LEFT,
            145
        );
        results_->InsertColumn(
            1,
            U("类型"),
            wxLIST_FORMAT_LEFT,
            90
        );
        results_->InsertColumn(
            2,
            U("标题"),
            wxLIST_FORMAT_LEFT,
            245
        );
        results_->InsertColumn(
            3,
            U("状态"),
            wxLIST_FORMAT_LEFT,
            105
        );
        results_->InsertColumn(
            4,
            U("摘要"),
            wxLIST_FORMAT_LEFT,
            330
        );

        resultsLayout->Add(
            results_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        resultsCard->SetSizer(resultsLayout);

        auto* previewCard = Card(this);
        auto* previewLayout = new wxBoxSizer(wxVERTICAL);

        previewLayout->Add(
            Label(
                previewCard,
                U("快速预览"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        previewTitle_ = Label(
            previewCard,
            U("选择一项查看内容"),
            15,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        );
        previewTitle_->Wrap(310);

        previewLayout->Add(
            previewTitle_,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        preview_ = Input(
            previewCard,
            wxTE_MULTILINE | wxTE_READONLY
        );
        preview_->SetMinSize(wxSize(330, -1));

        previewLayout->Add(
            preview_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        open_ = Button(
            previewCard,
            U("打开完整详情"),
            true
        );
        open_->Enable(false);

        previewLayout->Add(
            open_,
            0,
            wxEXPAND | wxALL,
            16
        );

        previewCard->SetSizer(previewLayout);
        previewCard->SetMinSize(wxSize(365, -1));

        body->Add(filters, 0, wxEXPAND | wxRIGHT, 14);
        body->Add(
            resultsCard,
            1,
            wxEXPAND | wxRIGHT,
            14
        );
        body->Add(previewCard, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 26);
        SetSizer(root);

        Bind(
            wxEVT_TIMER,
            &GlobalSearchPage::OnCompletionTimer,
            this,
            completionTimer_.GetId()
        );

        searchButton_->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                RequestSearch();
            }
        );

        query_->Bind(
            wxEVT_TEXT_ENTER,
            [this](wxCommandEvent&)
            {
                RequestSearch();
            }
        );

        type_->Bind(
            wxEVT_CHOICE,
            [this](wxCommandEvent&)
            {
                if (!query_->GetValue().empty())
                {
                    RequestSearch();
                }
            }
        );

        statusFilter_->Bind(
            wxEVT_CHOICE,
            [this](wxCommandEvent&)
            {
                if (!query_->GetValue().empty())
                {
                    RequestSearch();
                }
            }
        );

        results_->Bind(
            wxEVT_LIST_ITEM_SELECTED,
            [this](wxListEvent& event)
            {
                Select(
                    static_cast<std::size_t>(
                        event.GetIndex()
                    )
                );
            }
        );

        results_->Bind(
            wxEVT_LIST_ITEM_ACTIVATED,
            [this](wxListEvent&)
            {
                OpenSelected();
            }
        );

        open_->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                OpenSelected();
            }
        );

        save->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                SaveQuery();
            }
        );

        advanced->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                type_->SetFocus();
            }
        );
    }

    ~GlobalSearchPage() override
    {
        shuttingDown_ = true;
        completionTimer_.Stop();

        /*
         * 异步任务捕获了 this，但只调用不访问 wx 控件的查询函数。
         * 必须在页面销毁前等待任务退出，防止成员和数据库引用悬空。
         */
        if (future_.valid())
        {
            future_.wait();
        }
    }

private:
    struct Result
    {
        std::string id;
        std::string kind;
        std::string subtype;
        std::string title;
        std::string state;
        std::string summary;
        std::string fileId;
    };

    struct Request
    {
        std::uint64_t generation = 0;
        std::string expression;
        std::string plain;
        std::string kind;
        std::string statusFilter;
        std::string objectType;
        std::string objectStatus;
        std::string owner;
    };

    struct Output
    {
        StorageStatus status = StorageStatus::Ok();
        std::uint64_t generation = 0;
        std::vector<Result> results;
    };

    static std::string ChoiceValue(
        wxChoice* choice
    )
    {
        const int selected =
            choice->GetSelection();

        if (selected == wxNOT_FOUND)
        {
            return {};
        }

        auto* data =
            dynamic_cast<wxStringClientData*>(
                choice->GetClientObject(selected)
            );

        return data == nullptr
            ? std::string()
            : Utf8(data->GetData());
    }

    static std::string TokenValue(
        const std::string& expression,
        const std::string& key
    )
    {
        std::istringstream stream(expression);
        std::string token;
        const std::string prefix = key + ":";

        while (stream >> token)
        {
            if (token.rfind(prefix, 0) == 0)
            {
                return token.substr(prefix.size());
            }
        }

        return {};
    }

    static std::string PlainQuery(
        const std::string& expression
    )
    {
        std::istringstream stream(expression);
        std::ostringstream output;
        std::string token;
        bool first = true;

        while (stream >> token)
        {
            if (token.find(':') != std::string::npos)
            {
                continue;
            }

            if (!first)
            {
                output << ' ';
            }

            output << token;
            first = false;
        }

        return output.str();
    }

    static void AppendResult(
        Output& output,
        Result result
    )
    {
        if (output.results.size() < 200)
        {
            output.results.push_back(
                std::move(result)
            );
        }
    }

    static StorageStatus SearchObjects(
        Database& database,
        const Request& request,
        Output& output
    )
    {
        std::lock_guard<std::recursive_mutex> lock(
            database.Mutex()
        );

        sqlite3* handle = database.Handle();

        if (handle == nullptr)
        {
            return StorageStatus::Error(
                SQLITE_MISUSE,
                "搜索期间数据库不可用"
            );
        }

        Statement statement(
            handle,
            "SELECT id, object_type, title, status, "
            "description, owner "
            "FROM objects "
            "WHERE deleted=0 "
            "AND (?='' OR object_type=?) "
            "AND (?='' OR status=?) "
            "AND (?='' OR owner LIKE '%'||?||'%') "
            "AND (?='' OR title LIKE '%'||?||'%' "
            "OR description LIKE '%'||?||'%' "
            "OR owner LIKE '%'||?||'%') "
            "ORDER BY updated_at DESC "
            "LIMIT 200;"
        );

        if (!statement.Valid())
        {
            return Error(
                handle,
                statement.Status(),
                "无法准备业务对象搜索"
            );
        }

        BindText(statement.Get(), 1, request.objectType);
        BindText(statement.Get(), 2, request.objectType);
        BindText(statement.Get(), 3, request.objectStatus);
        BindText(statement.Get(), 4, request.objectStatus);
        BindText(statement.Get(), 5, request.owner);
        BindText(statement.Get(), 6, request.owner);
        BindText(statement.Get(), 7, request.plain);
        BindText(statement.Get(), 8, request.plain);
        BindText(statement.Get(), 9, request.plain);
        BindText(statement.Get(), 10, request.plain);

        int step = SQLITE_ROW;

        while ((step = sqlite3_step(statement.Get())) ==
               SQLITE_ROW)
        {
            std::string summary =
                Text(statement.Get(), 4);

            const std::string owner =
                Text(statement.Get(), 5);

            if (!owner.empty())
            {
                if (!summary.empty())
                {
                    summary += "\n";
                }

                summary += "负责人：" + owner;
            }

            AppendResult(
                output,
                {
                    Text(statement.Get(), 0),
                    "object",
                    Text(statement.Get(), 1),
                    Text(statement.Get(), 2),
                    Text(statement.Get(), 3),
                    summary,
                    {}
                }
            );
        }

        return step == SQLITE_DONE
            ? StorageStatus::Ok()
            : Error(
                handle,
                step,
                "业务对象搜索失败"
            );
    }

    static StorageStatus SearchEvidence(
        Database& database,
        const Request& request,
        Output& output
    )
    {
        std::string requestedState;

        if (request.statusFilter == "review")
        {
            requestedState = "review";
        }
        else if (request.statusFilter == "active")
        {
            requestedState = "verified";
        }
        else if (request.statusFilter == "closed")
        {
            requestedState = "rejected";
        }

        std::lock_guard<std::recursive_mutex> lock(
            database.Mutex()
        );

        sqlite3* handle = database.Handle();

        if (handle == nullptr)
        {
            return StorageStatus::Error(
                SQLITE_MISUSE,
                "搜索期间数据库不可用"
            );
        }

        Statement statement(
            handle,
            "SELECT id, title, review_state, quote, file_id "
            "FROM evidence "
            "WHERE deleted=0 "
            "AND ("
            "?='' OR "
            "(?='review' AND review_state IN "
            "('unverified','needs_review','changed')) OR "
            "review_state=?"
            ") "
            "AND (?='' OR title LIKE '%'||?||'%' "
            "OR quote LIKE '%'||?||'%') "
            "ORDER BY updated_at DESC "
            "LIMIT 200;"
        );

        if (!statement.Valid())
        {
            return Error(
                handle,
                statement.Status(),
                "无法准备证据搜索"
            );
        }

        BindText(statement.Get(), 1, requestedState);
        BindText(statement.Get(), 2, requestedState);
        BindText(statement.Get(), 3, requestedState);
        BindText(statement.Get(), 4, request.plain);
        BindText(statement.Get(), 5, request.plain);
        BindText(statement.Get(), 6, request.plain);

        int step = SQLITE_ROW;

        while ((step = sqlite3_step(statement.Get())) ==
               SQLITE_ROW)
        {
            AppendResult(
                output,
                {
                    Text(statement.Get(), 0),
                    "evidence",
                    "证据",
                    Text(statement.Get(), 1),
                    Text(statement.Get(), 2),
                    Text(statement.Get(), 3),
                    Text(statement.Get(), 4)
                }
            );
        }

        return step == SQLITE_DONE
            ? StorageStatus::Ok()
            : Error(
                handle,
                step,
                "证据搜索失败"
            );
    }

    static StorageStatus SearchFiles(
        Database& database,
        const Request& request,
        Output& output
    )
    {
        if (request.plain.empty())
        {
            return StorageStatus::Ok();
        }

        SearchRequest searchRequest;
        searchRequest.query = request.plain;
        searchRequest.limit = 200;

        const SearchResponse response =
            SearchService(database).Search(
                searchRequest
            );

        if (!response.status.success)
        {
            return response.status;
        }

        for (const auto& hit : response.hits)
        {
            AppendResult(
                output,
                {
                    hit.fileId,
                    "file",
                    "文档",
                    hit.title.empty()
                        ? hit.displayName
                        : hit.title,
                    hit.mediaType,
                    hit.snippet,
                    hit.fileId
                }
            );
        }

        return StorageStatus::Ok();
    }

    static Output ExecuteSearch(
        Database& database,
        const Request& request
    )
    {
        Output output;
        output.generation = request.generation;

        if (request.kind.empty() ||
            request.kind == "object")
        {
            output.status = SearchObjects(
                database,
                request,
                output
            );

            if (!output.status.success)
            {
                return output;
            }
        }

        if (request.kind.empty() ||
            request.kind == "evidence")
        {
            output.status = SearchEvidence(
                database,
                request,
                output
            );

            if (!output.status.success)
            {
                return output;
            }
        }

        if (request.kind.empty() ||
            request.kind == "file")
        {
            output.status = SearchFiles(
                database,
                request,
                output
            );
        }

        return output;
    }

    Request BuildRequest()
    {
        Request request;
        request.generation = ++requestedGeneration_;
        request.expression =
            Utf8(query_->GetValue());
        request.plain =
            PlainQuery(request.expression);
        request.kind =
            ChoiceValue(type_);
        request.statusFilter =
            ChoiceValue(statusFilter_);
        request.objectType =
            TokenValue(
                request.expression,
                "type"
            );
        request.objectStatus =
            TokenValue(
                request.expression,
                "status"
            );
        request.owner =
            TokenValue(
                request.expression,
                "owner"
            );

        if (request.objectStatus.empty())
        {
            if (request.statusFilter == "active")
            {
                request.objectStatus = "active";
            }
            else if (
                request.statusFilter == "closed"
            )
            {
                request.objectStatus = "closed";
            }
        }

        return request;
    }

    void RequestSearch()
    {
        wxString value = query_->GetValue();
        value.Trim(true).Trim(false);

        if (value.empty())
        {
            resultStatus_->SetLabel(
                U("请输入搜索内容")
            );
            query_->SetFocus();
            return;
        }

        pendingRequest_ = BuildRequest();

        if (future_.valid())
        {
            resultStatus_->SetLabel(
                U("当前搜索结束后将执行最新查询……")
            );
            return;
        }

        LaunchPendingRequest();
    }

    void LaunchPendingRequest()
    {
        if (shuttingDown_)
        {
            return;
        }

        auto& workspace =
            WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            resultStatus_->SetLabel(
                U("工作区不可用")
            );
            return;
        }

        const Request request =
            pendingRequest_;

        Database* database =
            &workspace.GetDatabase();

        resultStatus_->SetLabel(
            U("正在搜索……")
        );
        searchButton_->Enable(false);

        future_ = std::async(
            std::launch::async,
            [database, request]()
            {
                return ExecuteSearch(
                    *database,
                    request
                );
            }
        );

        if (!completionTimer_.IsRunning())
        {
            completionTimer_.Start(50);
        }
    }

    void OnCompletionTimer(wxTimerEvent&)
    {
        if (!future_.valid())
        {
            completionTimer_.Stop();
            searchButton_->Enable(true);
            return;
        }

        if (future_.wait_for(
                std::chrono::milliseconds(0)
            ) != std::future_status::ready)
        {
            return;
        }

        Output output = future_.get();

        if (shuttingDown_)
        {
            completionTimer_.Stop();
            return;
        }

        if (output.generation !=
            requestedGeneration_)
        {
            LaunchPendingRequest();
            return;
        }

        completionTimer_.Stop();
        searchButton_->Enable(true);
        ApplyOutput(std::move(output));
    }

    void ApplyOutput(Output output)
    {
        resultsData_.clear();
        results_->Freeze();
        results_->DeleteAllItems();

        preview_->Clear();
        previewTitle_->SetLabel(
            U("选择一项查看内容")
        );
        open_->Enable(false);

        selected_ =
            static_cast<std::size_t>(-1);

        if (!output.status.success)
        {
            results_->Thaw();
            resultStatus_->SetLabel(
                U("搜索失败")
            );

            ShowStorageError(
                this,
                U("全局搜索"),
                output.status
            );
            return;
        }

        resultsData_ =
            std::move(output.results);

        for (const auto& value : resultsData_)
        {
            const long row = results_->InsertItem(
                results_->GetItemCount(),
                Wx(value.id)
            );

            results_->SetItem(
                row,
                1,
                Wx(value.subtype)
            );
            results_->SetItem(
                row,
                2,
                Wx(value.title)
            );
            results_->SetItem(
                row,
                3,
                Wx(value.state)
            );
            results_->SetItem(
                row,
                4,
                Wx(value.summary)
            );
        }

        results_->Thaw();

        resultStatus_->SetLabel(
            wxString::Format(
                U("%d 项 · 最多显示 200 项"),
                static_cast<int>(
                    resultsData_.size()
                )
            )
        );

        if (!resultsData_.empty())
        {
            results_->SetItemState(
                0,
                wxLIST_STATE_SELECTED |
                    wxLIST_STATE_FOCUSED,
                wxLIST_STATE_SELECTED |
                    wxLIST_STATE_FOCUSED
            );

            Select(0);
        }

        /*
         * 仅最终展示的查询写一次审计。
         * 已被新请求淘汰的查询不会写入审计。
         */
        auto& workspace =
            WorkspaceService::Instance();

        if (workspace.IsInitialized())
        {
            workspace.Audit().Append(
                "ui",
                "search",
                "query",
                "search_index",
                "global",
                "{\"results\":" +
                    std::to_string(
                        resultsData_.size()
                    ) +
                    "}"
            );
        }
    }

    void Select(std::size_t index)
    {
        if (index >= resultsData_.size())
        {
            return;
        }

        selected_ = index;
        const auto& result =
            resultsData_[index];

        previewTitle_->SetLabel(
            Wx(result.id + "  " + result.title)
        );

        preview_->ChangeValue(
            U("类型：") + Wx(result.subtype) +
            U("\n状态：") + Wx(result.state) +
            U("\n\n") + Wx(result.summary)
        );

        open_->Enable(true);
    }

    void OpenSelected()
    {
        if (selected_ >= resultsData_.size())
        {
            return;
        }

        const Result result =
            resultsData_[selected_];

        wxString content;

        if (result.kind == "object")
        {
            const auto record =
                WorkspaceService::Instance().
                    Objects().FindById(
                        result.id,
                        false
                    );

            if (record)
            {
                content =
                    U("编号：") + Wx(record->id) +
                    U("\n类型：") +
                        Wx(record->objectType) +
                    U("\n状态：") +
                        Wx(record->status) +
                    U("\n优先级：") +
                        Wx(record->priority) +
                    U("\n负责人：") +
                        Wx(record->owner) +
                    U("\n发生时间：") +
                        Wx(record->occurredAt) +
                    U("\n已知时间：") +
                        Wx(record->knownAt) +
                    U("\n有效期起：") +
                        Wx(record->validFrom) +
                    U("\n有效期止：") +
                        Wx(record->validTo) +
                    U("\n\n") +
                        Wx(record->description);
            }
        }
        else if (result.kind == "evidence")
        {
            const auto record =
                WorkspaceService::Instance().
                    Evidence().FindById(
                        result.id,
                        false
                    );

            if (record)
            {
                content =
                    U("编号：") + Wx(record->id) +
                    U("\n审查状态：") +
                        Wx(record->reviewState) +
                    U("\n来源文件：") +
                        Wx(record->fileId) +
                    U("\n锚点：") +
                        Wx(record->anchor) +
                    U("\n指纹：") +
                        Wx(record->fingerprint) +
                    U("\n\n引用内容：\n") +
                        Wx(record->quote);
            }
        }
        else
        {
            const ParsedFileContent parsed =
                UiDataService::Instance().
                    LoadParsedFile(
                        result.fileId,
                        2 * 1024 * 1024,
                        500
                    );

            if (parsed.status.success)
            {
                content =
                    U("文件：") +
                        Wx(parsed.file.displayName) +
                    U("\n相对路径：") +
                        Wx(parsed.file.relativePath) +
                    U("\n媒体类型：") +
                        Wx(parsed.file.mediaType) +
                    U("\n解析状态：") +
                        Wx(parsed.file.parseState) +
                    U("\n\n") +
                        Wx(parsed.content);
            }
            else
            {
                content =
                    Wx(parsed.status.message);
            }
        }

        wxDialog dialog(
            this,
            wxID_ANY,
            Wx(result.title),
            wxDefaultPosition,
            wxSize(1000, 720),
            wxDEFAULT_DIALOG_STYLE |
                wxRESIZE_BORDER
        );

        Theme::Apply(
            &dialog,
            Theme::Surface()
        );

        auto* layout =
            new wxBoxSizer(wxVERTICAL);

        auto* detail = new wxTextCtrl(
            &dialog,
            wxID_ANY,
            content,
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE |
                wxTE_READONLY |
                wxBORDER_NONE
        );

        detail->SetBackgroundColour(
            Theme::Input()
        );
        detail->SetForegroundColour(
            Theme::Text()
        );
        detail->SetFont(Theme::Font(10));

        auto* close =
            Button(&dialog, U("关闭"), true);
        close->SetId(wxID_OK);

        layout->Add(
            detail,
            1,
            wxEXPAND | wxALL,
            20
        );
        layout->Add(
            close,
            0,
            wxALIGN_RIGHT |
                wxLEFT |
                wxRIGHT |
                wxBOTTOM,
            20
        );

        dialog.SetSizer(layout);
        dialog.ShowModal();
    }

    void SaveQuery()
    {
        if (query_->GetValue().empty())
        {
            return;
        }

        const wxString name =
            wxGetTextFromUser(
                U("输入查询名称"),
                U("保存查询"),
                query_->GetValue(),
                this
            );

        if (name.empty())
        {
            return;
        }

        auto& workspace =
            WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            return;
        }

        Database& database =
            workspace.GetDatabase();

        const std::string id =
            NewId("QUERY");

        const auto status =
            database.Transaction([&]() {
                sqlite3* handle =
                    database.Handle();

                Statement statement(
                    handle,
                    "INSERT INTO saved_queries("
                    "id, name, description, query_json, "
                    "pinned, schedule"
                    ") VALUES(?, ?, '', ?, 0, '');"
                );

                if (!statement.Valid())
                {
                    return Error(
                        handle,
                        statement.Status(),
                        "无法准备查询保存"
                    );
                }

                BindText(statement.Get(), 1, id);
                BindText(
                    statement.Get(),
                    2,
                    Utf8(name)
                );
                BindText(
                    statement.Get(),
                    3,
                    Utf8(query_->GetValue())
                );

                const int result =
                    sqlite3_step(statement.Get());

                if (result != SQLITE_DONE)
                {
                    return Error(
                        handle,
                        result,
                        "无法保存查询"
                    );
                }

                return workspace.Audit().Append(
                    "ui",
                    "saved_query",
                    "save",
                    "saved_query",
                    id,
                    "{}"
                );
            });

        if (!status.success)
        {
            ShowStorageError(
                this,
                U("保存查询"),
                status
            );
        }
    }

    wxTextCtrl* query_ = nullptr;
    wxChoice* type_ = nullptr;
    wxChoice* statusFilter_ = nullptr;
    wxListCtrl* results_ = nullptr;
    wxStaticText* resultStatus_ = nullptr;
    wxStaticText* previewTitle_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    wxButton* open_ = nullptr;
    wxButton* searchButton_ = nullptr;

    std::vector<Result> resultsData_;
    std::size_t selected_ =
        static_cast<std::size_t>(-1);

    wxTimer completionTimer_;
    std::future<Output> future_;
    Request pendingRequest_;
    std::uint64_t requestedGeneration_ = 0;
    bool shuttingDown_ = false;
};



class RecycleBinPage final : public wxPanel
{
public:
    explicit RecycleBinPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* restore =
            Button(this, U("恢复所选"));

        auto* purge =
            Button(
                this,
                U("彻底删除所选"),
                false,
                true
            );

        auto* clear =
            Button(
                this,
                U("清空回收站"),
                false,
                true
            );

        actions->Add(restore, 0, wxRIGHT, 10);
        actions->Add(purge, 0, wxRIGHT, 10);
        actions->Add(clear, 0);

        AddHeading(
            this,
            root,
            U("P17"),
            U("回收站"),
            U("恢复或彻底清理已软删除的文件记录、证据、实体和业务对象"),
            actions
        );

        auto* card = Card(this);
        auto* layout = new wxBoxSizer(wxVERTICAL);
        auto* filters = new wxBoxSizer(wxHORIZONTAL);

        search_ = Input(card, wxTE_PROCESS_ENTER);
        search_->SetHint(U("搜索已删除内容"));

        type_ = new wxChoice(card, wxID_ANY);
        type_->Append(
            U("类型：全部"),
            new wxStringClientData("")
        );
        type_->Append(
            U("业务对象"),
            new wxStringClientData("object")
        );
        type_->Append(
            U("证据"),
            new wxStringClientData("evidence")
        );
        type_->Append(
            U("文件记录"),
            new wxStringClientData("file")
        );
        type_->Append(
            U("实体"),
            new wxStringClientData("entity")
        );
        type_->SetSelection(0);

        filters->Add(search_, 1, wxRIGHT, 14);
        filters->Add(type_, 0);

        layout->Add(
            filters,
            0,
            wxEXPAND | wxALL,
            16
        );

        list_ = new wxListCtrl(
            card,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxLC_REPORT | wxBORDER_NONE
        );

        list_->SetBackgroundColour(Theme::Input());
        list_->SetForegroundColour(Theme::Text());
        list_->SetFont(Theme::Font(9));
        list_->InsertColumn(
            0,
            U("编号"),
            wxLIST_FORMAT_LEFT,
            175
        );
        list_->InsertColumn(
            1,
            U("对象"),
            wxLIST_FORMAT_LEFT,
            300
        );
        list_->InsertColumn(
            2,
            U("类型"),
            wxLIST_FORMAT_LEFT,
            110
        );
        list_->InsertColumn(
            3,
            U("删除时间"),
            wxLIST_FORMAT_LEFT,
            165
        );
        list_->InsertColumn(
            4,
            U("删除原因"),
            wxLIST_FORMAT_LEFT,
            260
        );
        list_->InsertColumn(
            5,
            U("引用影响"),
            wxLIST_FORMAT_LEFT,
            150
        );

        layout->Add(
            list_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT,
            16
        );

        status_ = Label(
            card,
            wxEmptyString,
            9,
            Theme::Muted()
        );

        auto* warning = Label(
            card,
            U(
                "彻底删除不可撤销；相关引用将按数据完整性规则"
                "在同一事务中清理并记录审计事件。"
            ),
            9,
            Theme::Red()
        );

        layout->Add(status_, 0, wxALL, 16);
        layout->Add(
            warning,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        card->SetSizer(layout);
        root->Add(card, 1, wxEXPAND | wxALL, 26);
        SetSizer(root);

        restore->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                RestoreSelected();
            }
        );

        purge->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                PurgeSelected();
            }
        );

        clear->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                PurgeAll();
            }
        );

        search_->Bind(
            wxEVT_TEXT_ENTER,
            [this](wxCommandEvent&)
            {
                RefreshData();
            }
        );

        type_->Bind(
            wxEVT_CHOICE,
            [this](wxCommandEvent&)
            {
                RefreshData();
            }
        );

        list_->Bind(
            wxEVT_LIST_ITEM_SELECTED,
            [this](wxListEvent&)
            {
                UpdateStatus();
            }
        );

        list_->Bind(
            wxEVT_LIST_ITEM_DESELECTED,
            [this](wxListEvent&)
            {
                UpdateStatus();
            }
        );

        RefreshData();
    }

private:
    struct Record
    {
        std::string id;
        std::string kind;
        std::string title;
        std::string type;
        std::string deletedAt;
        std::string reason;
        int references = 0;
    };

    using ReasonMap =
        std::map<std::string, std::string>;

    std::string TypeValue() const
    {
        const int selected =
            type_->GetSelection();

        if (selected == wxNOT_FOUND)
        {
            return {};
        }

        auto* data =
            dynamic_cast<wxStringClientData*>(
                type_->GetClientObject(selected)
            );

        return data == nullptr
            ? std::string()
            : Utf8(data->GetData());
    }

    static bool Contains(
        const std::string& value,
        const wxString& query
    )
    {
        return query.empty() ||
            Wx(value).Lower().Contains(
                query.Lower()
            );
    }

    static bool TableExists(
        sqlite3* handle,
        const std::string& name
    )
    {
        Statement statement(
            handle,
            "SELECT 1 FROM sqlite_master "
            "WHERE (type='table' OR type='view') "
            "AND name=? LIMIT 1;"
        );

        if (!statement.Valid())
        {
            return false;
        }

        BindText(statement.Get(), 1, name);

        return sqlite3_step(statement.Get()) ==
            SQLITE_ROW;
    }

    static StorageStatus Execute(
        sqlite3* handle,
        const std::string& sql,
        const std::vector<std::string>& values
    )
    {
        Statement statement(handle, sql);

        if (!statement.Valid())
        {
            return Error(
                handle,
                statement.Status(),
                "无法准备回收站操作"
            );
        }

        for (std::size_t index = 0;
             index < values.size();
             ++index)
        {
            BindText(
                statement.Get(),
                static_cast<int>(index + 1),
                values[index]
            );
        }

        const int result =
            sqlite3_step(statement.Get());

        return result == SQLITE_DONE
            ? StorageStatus::Ok()
            : Error(
                handle,
                result,
                "无法执行回收站操作"
            );
    }

    static StorageStatus ExecuteOne(
        sqlite3* handle,
        const std::string& sql,
        const std::string& id
    )
    {
        return Execute(handle, sql, {id});
    }

    static ReasonMap ReadReasons(
        Database& database
    )
    {
        ReasonMap reasons;

        /*
         * 审计只读取一次，避免对每条回收站记录重复查询。
         */
        const auto audits =
            AuditRepository(database).Recent(5000);

        for (const auto& audit : audits)
        {
            if (audit.objectId.empty())
            {
                continue;
            }

            if (audit.action != "soft_delete" &&
                audit.action != "delete")
            {
                continue;
            }

            if (reasons.find(audit.objectId) ==
                reasons.end())
            {
                reasons.emplace(
                    audit.objectId,
                    audit.payload
                );
            }
        }

        return reasons;
    }

    void AddRows(
        Database& database,
        const std::string& kind,
        const ReasonMap& reasons
    )
    {
        sqlite3* handle = database.Handle();
        std::string sql;

        if (kind == "object")
        {
            sql =
                "SELECT o.id, o.title, o.object_type, "
                "o.updated_at, "
                "(SELECT count(*) FROM object_evidence x "
                "WHERE x.object_id=o.id) + "
                "(SELECT count(*) FROM object_relations r "
                "WHERE r.source_object_id=o.id "
                "OR r.target_object_id=o.id) + "
                "(SELECT count(*) FROM handover_items h "
                "WHERE h.object_id=o.id) "
                "FROM objects o "
                "WHERE o.deleted=1 "
                "ORDER BY o.updated_at DESC "
                "LIMIT 500;";
        }
        else if (kind == "evidence")
        {
            sql =
                "SELECT e.id, e.title, 'evidence', "
                "e.updated_at, "
                "(SELECT count(*) FROM object_evidence x "
                "WHERE x.evidence_id=e.id) + "
                "(SELECT count(*) "
                "FROM change_review_evidence c "
                "WHERE c.evidence_id=e.id) + "
                "(SELECT count(*) FROM conflict_claims k "
                "WHERE k.evidence_id=e.id) "
                "FROM evidence e "
                "WHERE e.deleted=1 "
                "ORDER BY e.updated_at DESC "
                "LIMIT 500;";
        }
        else if (kind == "file")
        {
            sql =
                "SELECT f.id, f.display_name, 'file', "
                "f.updated_at, "
                "(SELECT count(*) FROM evidence e "
                "WHERE e.file_id=f.id) + "
                "(SELECT count(*) FROM file_versions v "
                "WHERE v.file_id=f.id) + "
                "(SELECT count(*) FROM change_reviews c "
                "WHERE c.file_id=f.id) "
                "FROM files f "
                "WHERE f.deleted=1 "
                "ORDER BY f.updated_at DESC "
                "LIMIT 500;";
        }
        else
        {
            sql =
                "SELECT id, canonical_name, entity_type, "
                "updated_at, 0 "
                "FROM entities "
                "WHERE deleted=1 "
                "ORDER BY updated_at DESC "
                "LIMIT 500;";
        }

        Statement statement(handle, sql);

        if (!statement.Valid())
        {
            return;
        }

        const wxString query =
            search_->GetValue();

        while (sqlite3_step(statement.Get()) ==
               SQLITE_ROW)
        {
            Record record;
            record.id = Text(statement.Get(), 0);
            record.title = Text(statement.Get(), 1);
            record.kind = kind;
            record.type = Text(statement.Get(), 2);
            record.deletedAt =
                Text(statement.Get(), 3);
            record.references =
                sqlite3_column_int(
                    statement.Get(),
                    4
                );

            if (!Contains(record.id, query) &&
                !Contains(record.title, query))
            {
                continue;
            }

            const auto reason =
                reasons.find(record.id);

            if (reason != reasons.end())
            {
                record.reason = reason->second;
            }

            records_.push_back(
                std::move(record)
            );
        }
    }

    void RefreshData()
    {
        records_.clear();
        list_->DeleteAllItems();

        auto& workspace =
            WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            status_->SetLabel(
                U("工作区不可用")
            );
            return;
        }

        Database& database =
            workspace.GetDatabase();

        const ReasonMap reasons =
            ReadReasons(database);

        {
            std::lock_guard<std::recursive_mutex> lock(
                database.Mutex()
            );

            const std::string type =
                TypeValue();

            if (type.empty() ||
                type == "object")
            {
                AddRows(
                    database,
                    "object",
                    reasons
                );
            }

            if (type.empty() ||
                type == "evidence")
            {
                AddRows(
                    database,
                    "evidence",
                    reasons
                );
            }

            if (type.empty() ||
                type == "file")
            {
                AddRows(
                    database,
                    "file",
                    reasons
                );
            }

            if (type.empty() ||
                type == "entity")
            {
                AddRows(
                    database,
                    "entity",
                    reasons
                );
            }
        }

        list_->Freeze();

        for (const auto& record : records_)
        {
            const long row = list_->InsertItem(
                list_->GetItemCount(),
                Wx(record.id)
            );

            list_->SetItem(
                row,
                1,
                Wx(record.title)
            );
            list_->SetItem(
                row,
                2,
                Wx(record.type)
            );
            list_->SetItem(
                row,
                3,
                Wx(record.deletedAt)
            );
            list_->SetItem(
                row,
                4,
                record.reason.empty()
                    ? U("用户删除")
                    : Wx(record.reason)
            );
            list_->SetItem(
                row,
                5,
                record.references == 0
                    ? U("无当前引用")
                    : wxString::Format(
                        U("%d 项引用"),
                        record.references
                    )
            );
        }

        list_->Thaw();
        UpdateStatus();
    }

    void UpdateStatus()
    {
        status_->SetLabel(
            wxString::Format(
                U("共 %d 项 · 已选择 %d 项"),
                static_cast<int>(
                    records_.size()
                ),
                static_cast<int>(
                    SelectedIndices().size()
                )
            )
        );
    }

    std::vector<std::size_t>
    SelectedIndices() const
    {
        std::vector<std::size_t> selected;
        long row = -1;

        while ((row = list_->GetNextItem(
                    row,
                    wxLIST_NEXT_ALL,
                    wxLIST_STATE_SELECTED
                )) != -1)
        {
            const auto index =
                static_cast<std::size_t>(row);

            if (index < records_.size())
            {
                selected.push_back(index);
            }
        }

        return selected;
    }

    StorageStatus RestoreInsideTransaction(
        sqlite3* handle,
        const Record& record
    )
    {
        StorageStatus status;

        if (record.kind == "object")
        {
            status = ExecuteOne(
                handle,
                "UPDATE objects SET "
                "deleted=0, "
                "updated_at=CURRENT_TIMESTAMP "
                "WHERE id=? AND deleted=1;",
                record.id
            );
        }
        else if (record.kind == "evidence")
        {
            status = ExecuteOne(
                handle,
                "UPDATE evidence SET "
                "deleted=0, "
                "updated_at=CURRENT_TIMESTAMP "
                "WHERE id=? AND deleted=1;",
                record.id
            );
        }
        else if (record.kind == "file")
        {
            status = ExecuteOne(
                handle,
                "UPDATE files SET "
                "deleted=0, "
                "parse_state='pending', "
                "updated_at=CURRENT_TIMESTAMP "
                "WHERE id=? AND deleted=1;",
                record.id
            );
        }
        else if (record.kind == "entity")
        {
            status = ExecuteOne(
                handle,
                "UPDATE entities SET "
                "deleted=0, "
                "updated_at=CURRENT_TIMESTAMP "
                "WHERE id=? AND deleted=1;",
                record.id
            );
        }
        else
        {
            return StorageStatus::Error(
                SQLITE_MISMATCH,
                "未知回收站记录类型"
            );
        }

        if (!status.success)
        {
            return status;
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "记录不存在或已经恢复"
            );
        }

        return WorkspaceService::Instance().
            Audit().Append(
                "ui",
                "recycle_bin",
                "restore",
                record.kind,
                record.id,
                "{}"
            );
    }

    StorageStatus PurgeInsideTransaction(
        sqlite3* handle,
        const Record& record
    )
    {
        StorageStatus status = StorageStatus::Ok();

        const auto run =
            [&](const std::string& sql,
                const std::vector<std::string>& values)
            {
                if (status.success)
                {
                    status = Execute(
                        handle,
                        sql,
                        values
                    );
                }
            };

        if (record.kind == "object")
        {
            run(
                "DELETE FROM object_evidence "
                "WHERE object_id=?;",
                {record.id}
            );

            run(
                "DELETE FROM object_tags "
                "WHERE object_id=?;",
                {record.id}
            );

            run(
                "DELETE FROM object_relations "
                "WHERE source_object_id=? "
                "OR target_object_id=?;",
                {record.id, record.id}
            );

            run(
                "DELETE FROM handover_items "
                "WHERE object_id=?;",
                {record.id}
            );

            run(
                "UPDATE conflicts SET "
                "subject_object_id=NULL, "
                "updated_at=CURRENT_TIMESTAMP "
                "WHERE subject_object_id=?;",
                {record.id}
            );

            run(
                "UPDATE conflict_claims SET "
                "object_id=NULL "
                "WHERE object_id=?;",
                {record.id}
            );

            run(
                "DELETE FROM objects "
                "WHERE id=? AND deleted=1;",
                {record.id}
            );
        }
        else if (record.kind == "evidence")
        {
            run(
                "DELETE FROM object_evidence "
                "WHERE evidence_id=?;",
                {record.id}
            );

            run(
                "DELETE FROM change_review_evidence "
                "WHERE evidence_id=?;",
                {record.id}
            );

            run(
                "UPDATE conflict_claims SET "
                "evidence_id=NULL "
                "WHERE evidence_id=?;",
                {record.id}
            );

            run(
                "DELETE FROM evidence "
                "WHERE id=? AND deleted=1;",
                {record.id}
            );
        }
        else if (record.kind == "file")
        {
            run(
                "UPDATE evidence SET "
                "file_id=NULL, "
                "review_state='needs_review', "
                "updated_at=CURRENT_TIMESTAMP "
                "WHERE file_id=?;",
                {record.id}
            );

            run(
                "DELETE FROM change_review_evidence "
                "WHERE review_id IN ("
                "SELECT id FROM change_reviews "
                "WHERE file_id=?"
                ");",
                {record.id}
            );

            run(
                "DELETE FROM change_reviews "
                "WHERE file_id=?;",
                {record.id}
            );

            run(
                "DELETE FROM file_versions "
                "WHERE file_id=?;",
                {record.id}
            );

            /*
             * 解析和索引表是按需创建的，只有存在时才执行。
             */
            if (status.success &&
                TableExists(
                    handle,
                    "parsed_sections"
                ))
            {
                run(
                    "DELETE FROM parsed_sections "
                    "WHERE file_id=?;",
                    {record.id}
                );
            }

            if (status.success &&
                TableExists(
                    handle,
                    "parsed_documents"
                ))
            {
                run(
                    "DELETE FROM parsed_documents "
                    "WHERE file_id=?;",
                    {record.id}
                );
            }

            if (status.success &&
                TableExists(
                    handle,
                    "search_fts"
                ))
            {
                run(
                    "DELETE FROM search_fts "
                    "WHERE file_id=?;",
                    {record.id}
                );
            }

            if (status.success &&
                TableExists(
                    handle,
                    "search_index_queue"
                ))
            {
                run(
                    "DELETE FROM search_index_queue "
                    "WHERE file_id=?;",
                    {record.id}
                );
            }

            run(
                "DELETE FROM files "
                "WHERE id=? AND deleted=1;",
                {record.id}
            );
        }
        else if (record.kind == "entity")
        {
            run(
                "DELETE FROM entities "
                "WHERE id=? AND deleted=1;",
                {record.id}
            );
        }
        else
        {
            return StorageStatus::Error(
                SQLITE_MISMATCH,
                "未知回收站记录类型"
            );
        }

        if (!status.success)
        {
            return status;
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "记录不存在、未处于删除状态或已被处理"
            );
        }

        return WorkspaceService::Instance().
            Audit().Append(
                "ui",
                "recycle_bin",
                "purge",
                record.kind,
                record.id,
                "{}"
            );
    }

    StorageStatus RestoreBatch(
        const std::vector<Record>& batch
    )
    {
        auto& workspace =
            WorkspaceService::Instance();

        Database& database =
            workspace.GetDatabase();

        return database.Transaction([&]() {
            sqlite3* handle =
                database.Handle();

            for (const auto& record : batch)
            {
                const auto status =
                    RestoreInsideTransaction(
                        handle,
                        record
                    );

                if (!status.success)
                {
                    return status;
                }
            }

            return StorageStatus::Ok();
        });
    }

    StorageStatus PurgeBatch(
        const std::vector<Record>& batch
    )
    {
        auto& workspace =
            WorkspaceService::Instance();

        Database& database =
            workspace.GetDatabase();

        return database.Transaction([&]() {
            sqlite3* handle =
                database.Handle();

            /*
             * 先删除对象和证据，再删除文件。
             * 文件删除会把尚未删除的证据来源置空并标记复核。
             */
            static const std::vector<std::string> order = {
                "object",
                "evidence",
                "entity",
                "file"
            };

            for (const auto& kind : order)
            {
                for (const auto& record : batch)
                {
                    if (record.kind != kind)
                    {
                        continue;
                    }

                    const auto status =
                        PurgeInsideTransaction(
                            handle,
                            record
                        );

                    if (!status.success)
                    {
                        return status;
                    }
                }
            }

            return StorageStatus::Ok();
        });
    }

    std::vector<Record> RecordsAt(
        const std::vector<std::size_t>& indices
    ) const
    {
        std::vector<Record> result;
        result.reserve(indices.size());

        for (const auto index : indices)
        {
            if (index < records_.size())
            {
                result.push_back(
                    records_[index]
                );
            }
        }

        return result;
    }

    void NotifyDataChanged()
    {
        UiDataService::Instance().Refresh(
            UiChangeType::Objects
        );

        UiDataService::Instance().Refresh(
            UiChangeType::Evidence
        );

        UiDataService::Instance().Refresh(
            UiChangeType::Files
        );

        UiDataService::Instance().Refresh(
            UiChangeType::Dashboard
        );
    }

    void RestoreSelected()
    {
        const auto selected =
            SelectedIndices();

        if (selected.empty())
        {
            wxMessageBox(
                U("请先选择要恢复的项目。"),
                U("回收站"),
                wxOK | wxICON_INFORMATION,
                this
            );
            return;
        }

        const auto status =
            RestoreBatch(
                RecordsAt(selected)
            );

        if (!status.success)
        {
            ShowStorageError(
                this,
                U("恢复失败"),
                status
            );
            return;
        }

        NotifyDataChanged();
        RefreshData();
    }

    void PurgeSelected()
    {
        const auto selected =
            SelectedIndices();

        if (selected.empty())
        {
            wxMessageBox(
                U("请先选择要彻底删除的项目。"),
                U("回收站"),
                wxOK | wxICON_INFORMATION,
                this
            );
            return;
        }

        if (wxMessageBox(
                U(
                    "彻底删除不可撤销。确认在一个原子事务中"
                    "删除所选项目及其必要引用吗？"
                ),
                U("确认彻底删除"),
                wxYES_NO |
                    wxNO_DEFAULT |
                    wxICON_WARNING,
                this
            ) != wxYES)
        {
            return;
        }

        const auto status =
            PurgeBatch(
                RecordsAt(selected)
            );

        if (!status.success)
        {
            ShowStorageError(
                this,
                U("彻底删除失败"),
                status
            );
            return;
        }

        NotifyDataChanged();
        RefreshData();
    }

    void PurgeAll()
    {
        if (records_.empty())
        {
            return;
        }

        if (wxMessageBox(
                U(
                    "确认在一个原子事务中彻底清空当前筛选范围"
                    "内的回收站内容？此操作不可撤销。"
                ),
                U("清空回收站"),
                wxYES_NO |
                    wxNO_DEFAULT |
                    wxICON_WARNING,
                this
            ) != wxYES)
        {
            return;
        }

        const auto status =
            PurgeBatch(records_);

        if (!status.success)
        {
            ShowStorageError(
                this,
                U("清空回收站失败"),
                status
            );
            return;
        }

        NotifyDataChanged();
        RefreshData();
    }

    wxTextCtrl* search_ = nullptr;
    wxChoice* type_ = nullptr;
    wxListCtrl* list_ = nullptr;
    wxStaticText* status_ = nullptr;
    std::vector<Record> records_;
};
