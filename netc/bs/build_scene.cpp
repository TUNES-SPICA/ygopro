#include "utils/common.h"
#include "utils/filesystem.h"

#include "../render_base.h"
#include "../sgui.h"
#include "../image_mgr.h"
#include "../card_data.h"
#include "../deck_data.h"

#include "build_scene_handler.h"
#include "build_scene.h"

namespace ygopro
{
    
    BuildScene::BuildScene() {
        limit[0] = ImageMgr::Get().GetTexture("limit0");
        limit[1] = ImageMgr::Get().GetTexture("limit1");
        limit[2] = ImageMgr::Get().GetTexture("limit2");
        pool[0] = ImageMgr::Get().GetTexture("pool_ocg");
        pool[1] = ImageMgr::Get().GetTexture("pool_tcg");
        pool[2] = ImageMgr::Get().GetTexture("pool_ex");
        hmask = ImageMgr::Get().GetTexture("cmask");
        result_data.fill(nullptr);
        bg_renderer = std::make_shared<base::SimpleTextureRenderer>();
        misc_renderer = std::make_shared<base::RenderObject2DLayout>();
        card_renderer = std::make_shared<base::RenderObject2DLayout>();
        PushObject(ImageMgr::Get());
        PushObject(bg_renderer.get());
        PushObject(misc_renderer.get());
        PushObject(misc_renderer.get());
    }
    
    BuildScene::~BuildScene() {
    }
    
    void BuildScene::Activate() {
        if(scene_handler)
            scene_handler->BeginHandler();
        RefreshAllCard();
    }
    
    bool BuildScene::Update() {
        if(input_handler)
            input_handler->UpdateInput();
        if(scene_handler)
            scene_handler->UpdateEvent();
        UpdateMisc();
        UpdateResult();
        return IsActive();
    }
    
    void BuildScene::Draw() {
        Render();
//        glViewport(0, 0, scene_size.x, scene_size.y);
//        auto& shader = base::Shader::GetDefaultShader();
//        shader.Use();
//        shader.SetParam1i("texid", 0);
//        // background
//        ImageMgr::Get().GetRawBGTexture()->Bind();
//        glBindVertexArray(back_vao);
//        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
//        GLCheckError(__FILE__, __LINE__);
//        // miscs
//        ImageMgr::Get().GetRawMiscTexture()->Bind();
//        glBindVertexArray(misc_vao);
//        glDrawElements(GL_TRIANGLES, 33 * 6, GL_UNSIGNED_SHORT, 0);
//        GLCheckError(__FILE__, __LINE__);
//        // cards
//        ImageMgr::Get().GetRawCardTexture()->Bind();
//        // result
//        if(result_show_size) {
//            glBindVertexArray(result_vao);
//            glDrawElements(GL_TRIANGLES, result_show_size * 24, GL_UNSIGNED_SHORT, 0);
//            GLCheckError(__FILE__, __LINE__);
//        }
//        // deck
//        size_t deck_sz = current_deck.main_deck.size() + current_deck.extra_deck.size() + current_deck.side_deck.size();
//        if(deck_sz > 0) {
//            glBindVertexArray(deck_vao);
//            glDrawElements(GL_TRIANGLES, deck_sz * 24, GL_UNSIGNED_SHORT, 0);
//        }
//        GLCheckError(__FILE__, __LINE__);
//        glBindVertexArray(0);
//        shader.Unuse();
    }
    
    void BuildScene::SetSceneSize(v2i sz) {
        scene_size = sz;
        float yrate = 1.0f - 40.0f / sz.y;
        card_size = {0.2f * yrate * sz.y / sz.x, 0.29f * yrate};
        icon_size = {0.08f * yrate * sz.y / sz.x, 0.08f * yrate};
        minx = 50.0f / sz.x * 2.0f - 1.0f;
        maxx = 0.541f;
        main_y_spacing = 0.3f * yrate;
        offsety[0] = (0.92f + 1.0f) * yrate - 1.0f;
        offsety[1] = (-0.31f + 1.0f) * yrate - 1.0f;
        offsety[2] = (-0.64f + 1.0f) * yrate - 1.0f;
        max_row_count = (maxx - minx - card_size.x) / (card_size.x * 1.1f);
        if(max_row_count < 10)
            max_row_count = 10;
        main_row_count = max_row_count;
        if((int32_t)current_deck.main_deck.size() > main_row_count * 4)
            main_row_count = (int32_t)((current_deck.main_deck.size() - 1) / 4 + 1);
        dx[0] = (maxx - minx - card_size.x) / (main_row_count - 1);
        int32_t rc1 = std::max((int32_t)current_deck.extra_deck.size(), max_row_count);
        dx[1] = (rc1 == 1) ? 0.0f : (maxx - minx - card_size.x) / (rc1 - 1);
        int32_t rc2 = std::max((int32_t)current_deck.side_deck.size(), max_row_count);
        dx[2] = (rc2 == 1) ? 0.0f : (maxx - minx - card_size.x) / (rc2 - 1);
        bg_renderer->SetScreenSize(sz);
        misc_renderer->SetScreenSize(sz);
        card_renderer->SetScreenSize(sz);
        bg_renderer->ClearVertices();
        bg_renderer->AddVertices(ImageMgr::Get().GetRawBGTexture(), {0, 0, sz.x, sz.y}, ImageMgr::Get().GetTexture("bg"));
        UpdateAllCard();
    }
    
    recti BuildScene::GetScreenshotClip() {
        int32_t maxx = (int32_t)(scene_size.x * 0.795f) - 1;
        return {0, 40, maxx, scene_size.y - 40};
    }
    
    void BuildScene::ShowSelectedInfo(uint32_t pos, uint32_t index) {
        if(pos > 0 && pos < 4) {
            auto dcd = GetCard(pos, index);
            if(dcd != nullptr)
                std::static_pointer_cast<BuildSceneHandler>(GetSceneHandler())->ShowCardInfo(dcd->data->code);
        } else if(pos == 4) {
            if(result_data[index] != 0)
                std::static_pointer_cast<BuildSceneHandler>(GetSceneHandler())->ShowCardInfo(result_data[index]->code);
        }
    }
    
    void BuildScene::ShowCardInfo(uint32_t code) {
        std::static_pointer_cast<BuildSceneHandler>(GetSceneHandler())->ShowCardInfo(code);
    }
    
    void BuildScene::HideCardInfo() {
        std::static_pointer_cast<BuildSceneHandler>(GetSceneHandler())->HideCardInfo();
    }
    
    void BuildScene::ClearDeck() {
        if(current_deck.main_deck.size() + current_deck.extra_deck.size() + current_deck.side_deck.size() == 0)
            return;
        for(auto& dcd : current_deck.main_deck)
            ImageMgr::Get().UnloadCardTexture(dcd->data->code);
        for(auto& dcd : current_deck.extra_deck)
            ImageMgr::Get().UnloadCardTexture(dcd->data->code);
        for(auto& dcd : current_deck.side_deck)
            ImageMgr::Get().UnloadCardTexture(dcd->data->code);
        current_deck.Clear();
        SetDeckDirty();
    }
    
    void BuildScene::SortDeck() {
        current_deck.Sort();
        RefreshAllIndex();
        UpdateAllCard();
    }
    
    void BuildScene::ShuffleDeck() {
        current_deck.Shuffle();
        RefreshAllIndex();
        UpdateAllCard();
    }
    
    void BuildScene::SetDeckDirty() {
        std::static_pointer_cast<BuildSceneHandler>(GetSceneHandler())->SetDeckEdited();
    }
    
    bool BuildScene::LoadDeckFromFile(const std::wstring& file) {
        DeckData tempdeck;
        if(tempdeck.LoadFromFile(FileSystem::WSTRToLocalFilename(file))) {
            ClearDeck();
            current_deck = tempdeck;
            for(auto& dcd : current_deck.main_deck) {
                auto exdata = std::make_shared<BuilderCard>();
                exdata->card_tex = ImageMgr::Get().GetCardTexture(dcd->data->code);
                exdata->show_exclusive = show_exclusive;
                dcd->extra = std::static_pointer_cast<DeckCardExtraData>(exdata);
            }
            for(auto& dcd : current_deck.extra_deck) {
                auto exdata = std::make_shared<BuilderCard>();
                exdata->card_tex = ImageMgr::Get().GetCardTexture(dcd->data->code);
                exdata->show_exclusive = show_exclusive;
                dcd->extra = std::static_pointer_cast<DeckCardExtraData>(exdata);
            }
            for(auto& dcd : current_deck.side_deck) {
                auto exdata = std::make_shared<BuilderCard>();
                exdata->card_tex = ImageMgr::Get().GetCardTexture(dcd->data->code);
                exdata->show_exclusive = show_exclusive;
                dcd->extra = std::static_pointer_cast<DeckCardExtraData>(exdata);
            }
            RefreshAllCard();
            return true;
        }
        return false;
    }
    
    bool BuildScene::LoadDeckFromString(const std::string& deck_string) {
        DeckData tempdeck;
        if(deck_string.find("ydk://") == 0 && tempdeck.LoadFromString(deck_string.substr(6))) {
            ClearDeck();
            current_deck = tempdeck;
            SetDeckDirty();
            for(auto& dcd : current_deck.main_deck) {
                auto exdata = std::make_shared<BuilderCard>();
                exdata->card_tex = ImageMgr::Get().GetCardTexture(dcd->data->code);
                exdata->show_exclusive = show_exclusive;
                dcd->extra = std::static_pointer_cast<DeckCardExtraData>(exdata);
            }
            for(auto& dcd : current_deck.extra_deck) {
                auto exdata = std::make_shared<BuilderCard>();
                exdata->card_tex = ImageMgr::Get().GetCardTexture(dcd->data->code);
                exdata->show_exclusive = show_exclusive;
                dcd->extra = std::static_pointer_cast<DeckCardExtraData>(exdata);
            }
            for(auto& dcd : current_deck.side_deck) {
                auto exdata = std::make_shared<BuilderCard>();
                exdata->card_tex = ImageMgr::Get().GetCardTexture(dcd->data->code);
                exdata->show_exclusive = show_exclusive;
                dcd->extra = std::static_pointer_cast<DeckCardExtraData>(exdata);
            }
            RefreshAllCard();
            return true;
        }
        return false;
    }
    
    bool BuildScene::SaveDeckToFile(const std::wstring& file) {
        auto deckfile = file;
        if(deckfile.find(L".ydk") != deckfile.length() - 4)
            deckfile.append(L".ydk");
        current_deck.SaveToFile(FileSystem::WSTRToLocalFilename(file));
        return true;
    }
    
    std::string BuildScene::SaveDeckToString() {
        return std::move(current_deck.SaveToString());
    }
    
    void BuildScene::UpdateAllCard() {
        size_t deck_sz = current_deck.main_deck.size() + current_deck.extra_deck.size() + current_deck.side_deck.size();
        if(deck_sz == 0)
            return;
        for(size_t i = 0; i < current_deck.main_deck.size(); ++i) {
            auto ptr = std::static_pointer_cast<BuilderCard>(current_deck.main_deck[i]->extra);
            auto bpos = ptr->pos;
            auto cpos = (v2f){minx + dx[0] * (i % main_row_count), offsety[0] - main_y_spacing * (i / main_row_count)};
            auto action = std::make_shared<LerpAnimator<int64_t, BuilderCard>>(1000, ptr, [bpos, cpos](BuilderCard* bc, double t) ->bool {
                bc->SetPos(bpos + (cpos - bpos) * t);
                return true;
            }, std::make_shared<TGenMove<int64_t>>(10));
        }
        for(size_t i = 0; i < current_deck.extra_deck.size(); ++i) {
            auto ptr = std::static_pointer_cast<BuilderCard>(current_deck.extra_deck[i]->extra);
            auto bpos = ptr->pos;
            auto cpos = (v2f){minx + dx[1] * i, offsety[1]};
            auto action = std::make_shared<LerpAnimator<int64_t, BuilderCard>>(1000, ptr, [bpos, cpos](BuilderCard* bc, double t) ->bool {
                bc->SetPos(bpos + (cpos - bpos) * t);
                return true;
            }, std::make_shared<TGenMove<int64_t>>(10));
        }
        for(size_t i = 0; i < current_deck.side_deck.size(); ++i) {
            auto ptr = std::static_pointer_cast<BuilderCard>(current_deck.side_deck[i]->extra);
            auto bpos = ptr->pos;
            auto cpos = (v2f){minx + dx[2] * i, offsety[2]};
            auto action = std::make_shared<LerpAnimator<int64_t, BuilderCard>>(1000, ptr, [bpos, cpos](BuilderCard* bc, double t) ->bool {
                bc->SetPos(bpos + (cpos - bpos) * t);
                return true;
            }, std::make_shared<TGenMove<int64_t>>(10));
        }
    }
    
    void BuildScene::RefreshParams() {
        main_row_count = max_row_count;
        if((int32_t)current_deck.main_deck.size() > main_row_count * 4)
            main_row_count = (int32_t)((current_deck.main_deck.size() - 1) / 4 + 1);
        dx[0] = (maxx - minx - card_size.x) / (main_row_count - 1);
        int32_t rc1 = std::max((int32_t)current_deck.extra_deck.size(), max_row_count);
        dx[1] = (rc1 == 1) ? 0.0f : (maxx - minx - card_size.x) / (rc1 - 1);
        int32_t rc2 = std::max((int32_t)current_deck.side_deck.size(), max_row_count);
        dx[2] = (rc2 == 1) ? 0.0f : (maxx - minx - card_size.x) / (rc2 - 1);
    }
    
    void BuildScene::RefreshAllCard() {
        size_t deck_sz = current_deck.main_deck.size() + current_deck.extra_deck.size() + current_deck.side_deck.size();
        if(deck_sz == 0)
            return;
        RefreshParams();
        for(size_t i = 0; i < current_deck.main_deck.size(); ++i) {
            auto cpos = (v2f){minx + dx[0] * (i % main_row_count), offsety[0] - main_y_spacing * (i / main_row_count)};
            auto ptr = std::static_pointer_cast<BuilderCard>(current_deck.main_deck[i]->extra);
            ptr->pos = cpos;
            ptr->size = card_size;
            RefreshCardPos(current_deck.main_deck[i]);
        }
        for(size_t i = 0; i < current_deck.extra_deck.size(); ++i) {
            auto cpos = (v2f){minx + dx[1] * i, offsety[1]};
            auto ptr = std::static_pointer_cast<BuilderCard>(current_deck.extra_deck[i]->extra);
            ptr->pos = cpos;
            ptr->size = card_size;
            RefreshCardPos(current_deck.extra_deck[i]);
        }
        for(size_t i = 0; i < current_deck.side_deck.size(); ++i) {
            auto cpos = (v2f){minx + dx[2] * i, offsety[2]};
            auto ptr = std::static_pointer_cast<BuilderCard>(current_deck.side_deck[i]->extra);
            ptr->pos = cpos;
            ptr->size = card_size;
            RefreshCardPos(current_deck.side_deck[i]);
        }
    }
    
    void BuildScene::RefreshCardPos(std::shared_ptr<DeckCardData> dcd) {
        auto ptr = std::static_pointer_cast<BuilderCard>(dcd->extra);
        std::array<base::v2ct, 16> verts;
        auto& pos = ptr->pos;
        auto& sz = ptr->size;
        base::FillVertex(&verts[0], ptr->pos, {sz.x, -sz.y}, ptr->card_tex);
        uint32_t cl = (((uint32_t)((float)ptr->hl * 255) & 0xff) << 24) | 0xffffff;
        base::FillVertex(&verts[4], ptr->pos, {sz.x, -sz.y}, hmask, cl);
        if((ptr->show_limit) && dcd->limit < 3) {
            auto& lti = limit[dcd->limit];
            base::FillVertex(&verts[8], pos + v2f{-0.01f, 0.01f}, {icon_size.x, -icon_size.y}, lti);
        }
        if((ptr->show_exclusive) && dcd->data->pool != 3) {
            float px = pos.x + sz.x / 2.0f - icon_size.x * 0.75f;
            auto& pti = (dcd->data->pool == 1) ? pool[0] : pool[1];
            base::FillVertex(&verts[12], {px, pos.y - sz.y + icon_size.y * 0.75f - 0.01f}, {icon_size.x * 1.5f, -icon_size.y * 0.75f}, pti);
        }
    }
    
    void BuildScene::UpdateMisc() {
        if(!update_misc)
            return;
        update_misc = false;
        std::array<base::v2ct, 33 * 4> verts;
        auto msk = ImageMgr::Get().GetTexture("mmask");
        auto nbk = ImageMgr::Get().GetTexture("numback");
        float yrate = 1.0f - 40.0f / scene_size.y;
        float lx = 10.0f / scene_size.x * 2.0f - 1.0f;
        float rx = 0.5625f;
        float y0 = (0.95f + 1.0f) * yrate - 1.0f;
        float y1 = (offsety[0] - main_y_spacing * 3 - card_size.y + offsety[1]) / 2;
        float y2 = (offsety[1] - card_size.y + offsety[2]) / 2;
        float y3 = offsety[2] - card_size.y - 0.03f * yrate;
        float nw = 60.0f / scene_size.x;
        float nh = 60.0f / scene_size.y;
        float nx = 15.0f / scene_size.x * 2.0f - 1.0f;
        float ndy = 64.0f / scene_size.y;
        float th = 120.0f / scene_size.y;
        float my = offsety[0] - main_y_spacing * 3 - card_size.y + th;
        float ey = offsety[1] - card_size.y + th;
        float sy = offsety[2] - card_size.y + th;
        auto numblock = [&nw, &nh, &nbk](base::v2ct* v, v2f pos, uint32_t cl1, uint32_t cl2, int32_t val) {
            base::FillVertex(&v[0], {pos.x, pos.y}, {nw, -nh}, nbk, cl1);
            if(val >= 10) {
                base::FillVertex(&v[4], {pos.x + nw * 0.1f, pos.y - nh * 0.2f}, {nw * 0.4f, -nh * 0.6f}, ImageMgr::Get().GetCharTex(L'0' + (val % 100) / 10), cl2);
                base::FillVertex(&v[8], {pos.x + nw * 0.5f, pos.y - nh * 0.2f}, {nw * 0.4f, -nh * 0.6f}, ImageMgr::Get().GetCharTex(L'0' + val % 10), cl2);
            } else
                base::FillVertex(&v[4], {pos.x + nw * 0.3f, pos.y - nh * 0.2f}, {nw * 0.4f, -nh * 0.6f}, ImageMgr::Get().GetCharTex(L'0' + val), cl2);
        };
        base::FillVertex(&verts[0], {lx, y0}, {rx - lx, y1 - y0}, msk, 0xc0ffffff);
        base::FillVertex(&verts[4], {lx, y1}, {rx - lx, y2 - y1}, msk, 0xc0c0c0c0);
        base::FillVertex(&verts[8], {lx, y2}, {rx - lx, y3 - y2}, msk, 0xc0808080);
        base::FillVertex(&verts[12], {nx, my}, {nw, -th}, ImageMgr::Get().GetTexture("main_t"), 0xff80ffff);
        base::FillVertex(&verts[16], {nx, ey}, {nw, -th}, ImageMgr::Get().GetTexture("extra_t"), 0xff80ffff);
        base::FillVertex(&verts[20], {nx, sy}, {nw, -th}, ImageMgr::Get().GetTexture("side_t"), 0xff80ffff);
        numblock(&verts[24], {nx, offsety[0] - ndy * 0}, 0xf03399ff, 0xff000000, current_deck.mcount);
        numblock(&verts[36], {nx, offsety[0] - ndy * 1}, 0xf0a0b858, 0xff000000, current_deck.scount);
        numblock(&verts[48], {nx, offsety[0] - ndy * 2}, 0xf09060bb, 0xff000000, current_deck.tcount);
        numblock(&verts[60], {nx, offsety[0] - ndy * 3}, 0xf0b050a0, 0xff000000, current_deck.fuscount);
        numblock(&verts[72], {nx, offsety[0] - ndy * 4}, 0xf0ffffff, 0xff000000, current_deck.syncount);
        numblock(&verts[84], {nx, offsety[0] - ndy * 5}, 0xf0000000, 0xffffffff, current_deck.xyzcount);
        numblock(&verts[96], {nx, my + card_size.y - th}, 0x80ffffff, 0xff000000, current_deck.main_deck.size());
        numblock(&verts[108], {nx, ey + card_size.y - th}, 0x80ffffff, 0xff000000, current_deck.extra_deck.size());
        numblock(&verts[120], {nx, sy + card_size.y - th}, 0x80ffffff, 0xff000000, current_deck.side_deck.size());
        glBindBuffer(GL_ARRAY_BUFFER, misc_buffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(base::v2ct) * 33 * 4, &verts[0]);
        GLCheckError(__FILE__, __LINE__);
    }
    
    void BuildScene::UpdateResult() {
        if(!update_result)
            return;
        update_result = false;
        result_show_size = 0;
        float left = 0.59f;
        float right = 1.0f - 10.0f / scene_size.x * 2.0f;
        float width = (right - left) / 2.0f;
        float top = 1.0f - 110.0f / scene_size.y * 2.0f;
        float down = 10.0f / scene_size.y * 2.0f - 1.0f;
        float height = (top - down) / 5.0f;
        float cheight = height * 0.9f;
        float cwidth = cheight / 2.9f * 2.0f * scene_size.y / scene_size.x;
        if(cwidth >= (right - left) / 2.0f) {
            cwidth = (right - left) / 2.0f;
            cheight = cwidth * 2.9f / 2.0f * scene_size.x / scene_size.y;
        }
        float offy = (height - cheight) * 0.5f;
        float iheight = 0.08f / 0.29f * cheight;
        float iwidth = iheight * scene_size.y / scene_size.x;
        std::array<base::v2ct, 160> verts;
        for(int32_t i = 0; i < 10 ; ++i) {
            if(result_data[i] == nullptr)
                continue;
            result_show_size++;
            auto pvert = &verts[i * 16];
            uint32_t cl = (i == current_sel_result) ? 0xc0ffffff : 0xc0000000;
            base::FillVertex(&pvert[0], {left + (i % 2) * width, top - (i / 2) * height}, {width, -height}, hmask, cl);
            CardData* pdata = result_data[i];
            base::FillVertex(&pvert[4], {left + (i % 2) * width + width / 2 - cwidth / 2, top - (i / 2) * height - offy}, {cwidth, -cheight}, result_tex[i]);
            uint32_t lmt = LimitRegulationMgr::Get().GetCardLimitCount(pdata->alias ? pdata->alias : pdata->code);
            if(lmt < 3) {
                base::FillVertex(&pvert[8], {left + (i % 2) * width + width / 2 - cwidth / 2 - 0.01f, top - (i / 2) * height - offy + 0.01f},
                                   {iwidth, -iheight}, limit[lmt]);
            }
            if(show_exclusive && pdata->pool != 3) {
                auto& pti = (pdata->pool == 1) ? pool[0] : pool[1];
                base::FillVertex(&pvert[12], {left + (i % 2) * width + width / 2 - iwidth * 0.75f, top - (i / 2) * height + offy - height + iheight * 0.75f - 0.01f},
                                   {iwidth * 1.5f, -iheight * 0.75f}, pti);
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER, result_buffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(base::v2ct) * result_show_size * 16, &verts[0]);
        GLCheckError(__FILE__, __LINE__);
    }
    
    void BuildScene::RefreshHL(std::shared_ptr<DeckCardData> dcd) {
        auto ptr = std::static_pointer_cast<BuilderCard>(dcd->extra);
        std::array<base::v2ct, 4> verts;
        uint32_t cl = (((uint32_t)(ptr->hl.Get() * 255) & 0xff) << 24) | 0xffffff;
        auto& sz = ptr->size.Get();
        base::FillVertex(&verts[0], ptr->pos.Get(), {sz.x, -sz.y}, hmask, cl);
        glBufferSubData(GL_ARRAY_BUFFER, sizeof(base::v2ct) * (ptr->buffer_index * 16 + 4), sizeof(base::v2ct) * 4, &verts[0]);
        GLCheckError(__FILE__, __LINE__);
    }
    
    void BuildScene::RefreshLimit(std::shared_ptr<DeckCardData> dcd) {
        auto ptr = std::static_pointer_cast<BuilderCard>(dcd->extra);
        std::array<base::v2ct, 4> verts;
        if((ptr->show_limit) && dcd->limit < 3) {
            auto lti = limit[dcd->limit];
            base::FillVertex(&verts[0], ptr->pos.Get() + v2f{-0.01f, 0.01f}, {icon_size.x, -icon_size.y}, lti);
        }
        glBufferSubData(GL_ARRAY_BUFFER, sizeof(base::v2ct) * (ptr->buffer_index * 16 + 8), sizeof(base::v2ct) * 4, &verts[0]);
        GLCheckError(__FILE__, __LINE__);
    }
    
    void BuildScene::RefreshEx(std::shared_ptr<DeckCardData> dcd) {
        auto ptr = std::static_pointer_cast<BuilderCard>(dcd->extra);
        std::array<base::v2ct, 4> verts;
        if((ptr->show_exclusive) && dcd->data->pool != 3) {
            auto& pos = ptr->pos.Get();
            auto& sz = ptr->size.Get();
            float px = pos.x + sz.x / 2.0f - icon_size.x * 0.75f;
            auto& pti = (dcd->data->pool == 1) ? pool[0] : pool[1];
            base::FillVertex(&verts[0], {px, pos.y - sz.y + icon_size.y * 0.75f - 0.01f}, {icon_size.x * 1.5f, -icon_size.y * 0.75f}, pti);
        }
        glBufferSubData(GL_ARRAY_BUFFER, sizeof(base::v2ct) * (ptr->buffer_index * 16 + 12), sizeof(base::v2ct) * 4, &verts[0]);
        GLCheckError(__FILE__, __LINE__);
    }
    
    void BuildScene::ChangeExclusive(bool check) {
        show_exclusive = check;
        for(auto& dcd : current_deck.main_deck) {
            auto ptr = std::static_pointer_cast<BuilderCard>(dcd->extra);
            ptr->show_exclusive = show_exclusive;
            RefreshEx(dcd);
        }
        for(auto& dcd : current_deck.extra_deck) {
            auto ptr = std::static_pointer_cast<BuilderCard>(dcd->extra);
            ptr->show_exclusive = show_exclusive;
            RefreshEx(dcd);
        }
        for(auto& dcd : current_deck.side_deck) {
            auto ptr = std::static_pointer_cast<BuilderCard>(dcd->extra);
            ptr->show_exclusive = show_exclusive;
            RefreshEx(dcd);
        }
        update_result = true;
    }
    
    void BuildScene::ChangeRegulation(int32_t index, int32_t view_regulation) {
        LimitRegulationMgr::Get().SetLimitRegulation(index);
        if(view_regulation)
            ViewRegulation(view_regulation - 1);
        else
            LimitRegulationMgr::Get().GetDeckCardLimitCount(current_deck);
        RefreshAllCard();
        update_result = true;
    }
    
    void BuildScene::ViewRegulation(int32_t limit) {
        ClearDeck();
        LimitRegulationMgr::Get().LoadCurrentListToDeck(current_deck, limit);
        for(auto& dcd : current_deck.main_deck) {
            auto exdata = std::make_shared<BuilderCard>();
            exdata->card_tex = ImageMgr::Get().GetCardTexture(dcd->data->code);
            exdata->show_exclusive = show_exclusive;
            dcd->extra = std::static_pointer_cast<DeckCardExtraData>(exdata);
        }
        for(auto& dcd : current_deck.extra_deck) {
            auto exdata = std::make_shared<BuilderCard>();
            exdata->card_tex = ImageMgr::Get().GetCardTexture(dcd->data->code);
            exdata->show_exclusive = show_exclusive;
            dcd->extra = std::static_pointer_cast<DeckCardExtraData>(exdata);
        }
        for(auto& dcd : current_deck.side_deck) {
            auto exdata = std::make_shared<BuilderCard>();
            exdata->card_tex = ImageMgr::Get().GetCardTexture(dcd->data->code);
            exdata->show_exclusive = show_exclusive;
            dcd->extra = std::static_pointer_cast<DeckCardExtraData>(exdata);
        }
        update_misc = true;
        RefreshAllCard();
    }
    
    void BuildScene::RefreshSearchResult(const std::array<CardData*, 10> new_results) {
        for(int32_t i = 0; i < 10; ++i) {
            if(new_results[i] != nullptr)
                result_tex[i] = ImageMgr::Get().GetCardTexture(new_results[i]->code);
        }
        for(int32_t i = 0; i < 10; ++i) {
            if(result_data[i] != nullptr)
                ImageMgr::Get().UnloadCardTexture(result_data[i]->code);
            result_data[i] = new_results[i];
        }
        update_result = true;
    }
    
    void BuildScene::SetCurrentSelection(int32_t sel, bool show_info) {
        if(sel != current_sel_result) {
            current_sel_result = sel;
            update_result = true;
            if(current_sel_result >= 0 && show_info && result_data[current_sel_result] != nullptr)
                GetSceneHandler<BuildSceneHandler>()->ShowCardInfo(result_data[current_sel_result]->code);
        }
    }
    
    void BuildScene::MoveCard(int32_t pos, int32_t index) {
        auto dcd = GetCard(pos, index);
        if(dcd == nullptr)
            return;
        if(pos == 1) {
            auto ptr = std::static_pointer_cast<BuilderCard>(current_deck.main_deck[index]->extra);
            ptr->hl.Reset(0.0f);
            current_deck.side_deck.push_back(current_deck.main_deck[index]);
            current_deck.main_deck.erase(current_deck.main_deck.begin() + index);
        } else if(pos == 2) {
            auto ptr = std::static_pointer_cast<BuilderCard>(current_deck.extra_deck[index]->extra);
            ptr->hl.Reset(0.0f);
            current_deck.side_deck.push_back(current_deck.extra_deck[index]);
            current_deck.extra_deck.erase(current_deck.extra_deck.begin() + index);
        } else if(dcd->data->type & 0x802040) {
            auto ptr = std::static_pointer_cast<BuilderCard>(current_deck.side_deck[index]->extra);
            ptr->hl.Reset(0.0f);
            current_deck.extra_deck.push_back(current_deck.side_deck[index]);
            current_deck.side_deck.erase(current_deck.side_deck.begin() + index);
        } else {
            auto ptr = std::static_pointer_cast<BuilderCard>(current_deck.side_deck[index]->extra);
            ptr->hl.Reset(0.0f);
            current_deck.main_deck.push_back(current_deck.side_deck[index]);
            current_deck.side_deck.erase(current_deck.side_deck.begin() + index);
        }
        current_deck.CalCount();
        RefreshParams();
        RefreshAllIndex();
        UpdateAllCard();
        SetDeckDirty();
        if(input_handler)
            input_handler->MouseMove({SceneMgr::Get().GetMousePosition().x, SceneMgr::Get().GetMousePosition().y});
    }
    
    void BuildScene::RemoveCard(int32_t pos, int32_t index) {
        if(update_status == 1)
            return;
        update_status = 1;
        auto dcd = GetCard(pos, index);
        if(dcd == nullptr)
            return;
        uint32_t code = dcd->data->code;
        auto ptr = std::static_pointer_cast<BuilderCard>(dcd->extra);
        v2f dst = ptr->pos.Get() + v2f{card_size.x / 2, -card_size.y / 2};
        v2f dsz = {0.0f, 0.0f};
        ptr->pos.SetAnimator(std::make_shared<LerpAnimator<v2f, TGenLinear>>(ptr->pos.Get(), dst, SceneMgr::Get().GetGameTime(), 0.2));
        ptr->size.SetAnimator(std::make_shared<LerpAnimator<v2f, TGenLinear>>(ptr->size.Get(), dsz, SceneMgr::Get().GetGameTime(), 0.2));
        ptr->show_limit = false;
        ptr->show_exclusive = false;
        AddUpdatingCard(dcd);
        GetSceneHandler<BuildSceneHandler>()->RegisterEvent([pos, index, code, this]() {
            if(current_deck.RemoveCard(pos, index)) {
                ImageMgr::Get().UnloadCardTexture(code);
                RefreshParams();
                RefreshAllIndex();
                UpdateAllCard();
                SetDeckDirty();
                if(input_handler)
                    input_handler->MouseMove({SceneMgr::Get().GetMousePosition().x, SceneMgr::Get().GetMousePosition().y});
                update_status = 0;
            }
        }, 0.2, 0, false);
    }
    
    void BuildScene::InsertSearchResult(int32_t index, bool is_side) {
        if(result_data[index] == nullptr)
            return;
        auto data = result_data[index];
        std::shared_ptr<DeckCardData> ptr;
        if(!is_side)
            ptr = current_deck.InsertCard(1, -1, data->code, true);
        else
            ptr = current_deck.InsertCard(3, -1, data->code, true);
        if(ptr != nullptr) {
            auto exdata = std::make_shared<BuilderCard>();
            exdata->card_tex = ImageMgr::Get().GetCardTexture(data->code);
            exdata->show_exclusive = show_exclusive;
            auto mpos = SceneMgr::Get().GetMousePosition();
            exdata->pos = (v2f){(float)mpos.x / scene_size.x * 2.0f - 1.0f, 1.0f - (float)mpos.y / scene_size.y * 2.0f};
            exdata->size = card_size;
            exdata->hl = 0.0f;
            ptr->extra = std::static_pointer_cast<DeckCardExtraData>(exdata);
            RefreshParams();
            RefreshAllIndex();
            UpdateAllCard();
            SetDeckDirty();
        }
    }
    
    std::shared_ptr<DeckCardData> BuildScene::GetCard(int32_t pos, int32_t index) {
        if(index < 0)
            return nullptr;
        if(pos == 1) {
            if(index >= (int32_t)current_deck.main_deck.size())
                return nullptr;
            return current_deck.main_deck[index];
        } else if(pos == 2) {
            if(index >= (int32_t)current_deck.extra_deck.size())
                return nullptr;
            return current_deck.extra_deck[index];
        } else if(pos == 3) {
            if(index >= (int32_t)current_deck.side_deck.size())
                return nullptr;
            return current_deck.side_deck[index];
        }
        return nullptr;
    }
    
    std::pair<int32_t, int32_t> BuildScene::GetHoverPos(int32_t posx, int32_t posy) {
        if(posx >= (int32_t)(scene_size.x * 0.795f) && posx <= scene_size.x - 10 && posy >= 110 && posy <= scene_size.y - 10) {
            int32_t sel = (int32_t)((posy - 110.0f) / ((scene_size.y - 120.0f) / 5.0f)) * 2;
            if(sel > 8)
                sel = 8;
            sel += (posx >= ((int32_t)(scene_size.x * 0.795f) + scene_size.x - 10) / 2) ? 1 : 0;
            return std::make_pair(4, sel);
        }
        float x = (float)posx / scene_size.x * 2.0f - 1.0f;
        float y = 1.0f - (float)posy / scene_size.y * 2.0f;
        if(x >= minx && x <= maxx) {
            if(y <= offsety[0] && y >= offsety[0] - main_y_spacing * 4) {
                uint32_t row = (uint32_t)((offsety[0] - y) / main_y_spacing);
                if(row > 3)
                    row = 3;
                int32_t index = 0;
                if(x > maxx - card_size.x)
                    index = main_row_count - 1;
                else
                    index = (int32_t)((x - minx) / dx[0]);
                int32_t cindex = index;
                index += row * main_row_count;
                if(index >= (int32_t)current_deck.main_deck.size())
                    return std::make_pair(0, 0);
                else {
                    if(y < offsety[0] - main_y_spacing * row - card_size.y || x > minx + cindex * dx[0] + card_size.x)
                        return std::make_pair(0, 0);
                    else
                        return std::make_pair(1, index);
                }
            } else if(y <= offsety[1] && y >= offsety[1] - card_size.y) {
                int32_t rc = std::max((int32_t)current_deck.extra_deck.size(), max_row_count);
                int32_t index = 0;
                if(x > maxx - card_size.x)
                    index = rc - 1;
                else
                    index = (int32_t)((x - minx) / dx[1]);
                if(index >= (int32_t)current_deck.extra_deck.size())
                    return std::make_pair(0, 0);
                else {
                    if(x > minx + index * dx[1] + card_size.x)
                        return std::make_pair(0, 0);
                    else
                        return std::make_pair(2, index);
                }
            } else if(y <= offsety[2] && y >= offsety[2] - card_size.y) {
                int32_t rc = std::max((int32_t)current_deck.side_deck.size(), max_row_count);
                int32_t index = 0;
                if(x > maxx - card_size.x)
                    index = rc - 1;
                else
                    index = (int32_t)((x - minx) / dx[2]);
                if(index >= (int32_t)current_deck.side_deck.size())
                    return std::make_pair(0, 0);
                else {
                    if(x > minx + index * dx[2] + card_size.x)
                        return std::make_pair(0, 0);
                    else
                        return std::make_pair(3, index);
                }
            }
        }
        return std::make_pair(0, 0);
    }
    
}