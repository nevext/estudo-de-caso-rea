# Estudo de caso: o que o REA realmente consegue fazer

Data dos testes: 11 de outubro de 2026.

## Pergunta

O REA consegue ler um programa pronto, sem código-fonte, bem o bastante para que ele seja reescrito? Somado a um agente como o Claude, isso basta para recriar um jogo, ou falta alguma coisa?

## Resposta curta

- **Sim, para a leitura.** Com REA + Ghidra, a lógica de um `.exe` sem nomes foi recuperada e reescrita em C; o programa reescrito deu saída idêntica à do original.
- **Só função por função.** Um programa real tem milhares de funções; o REA lê qualquer uma, mas entender todas continua sendo o trabalho longo.
- **REA + agente cobre a lógica.** O REA lê, o agente (Claude ou outro) interpreta, reescreve e testa. O que ainda falta para um jogo completo está na seção "REA + agente: o que ainda falta".
- **Para jogos .NET, a ferramenta é o ILSpy, não o REA.** No Stardew Valley o REA voltou vazio e o ILSpy devolveu 357.920 linhas de C# legível.
- **O uso mais promissor não é jogo.** É o resgate de sistemas legados: recuperar regras e dados de programas antigos sem código-fonte, inclusive na saúde. Ver "Usos além de jogos".

Este é um estudo de caso independente, feito para fins de estudo, sem vínculo com os autores do REA.

## Ambiente

| Item | Versão |
| --- | --- |
| REA (`rea-agents`) | 6.4.0, pelo terminal |
| Ghidra | 12.1.4, em `C:\tools\ghidra_12.1.4_PUBLIC` (variável de usuário `GHIDRA_INSTALL_DIR`) |
| adb (platform-tools) | Baixado, ainda não extraído nem configurado (`REA_ADB_PATH`); só é preciso para testes em Android |
| Java | 21.0.12 |
| Node.js | 24.12.0 |
| Compilador | gcc 15.2.0 (MSYS2 UCRT64) |
| Sistema | Windows 11, x64 |
| ILSpy e `ilspycmd` | 11.1.0 |
| dnSpy | 6.6.0 |
| dotPeek | 2026.2.3 |
| SDK do .NET | 10.0.401 |

O servidor MCP do REA não conectou na sessão (timeout de 30 s), então todos os testes foram feitos pela linha de comando.

## Teste principal: reconstruir um `.exe` nativo

### Alvo

Um mini-jogo em C escrito para o teste ([game.c](game.c)): bola, raquete, colisão, pontuação e cálculo de pan do som. Compilado com `gcc -O2 -s`, que remove todos os nomes de funções e variáveis, como num jogo comercial. O `.exe` ficou com 18.944 bytes.

Usar um alvo com código-fonte conhecido permite conferir o resultado.

### Passos

Todos com `GHIDRA_INSTALL_DIR` apontando para o Ghidra.

| # | Comando | O que voltou |
| --- | --- | --- |
| 1 | `rea analyze game_stripped.exe` | 114 funções, 102 strings, 11 segmentos (64 s) |
| 2 | `rea search game_stripped.exe score` | A string `pan=%d score=%u lives=%d` em `0x140004000` (51 s) |
| 3 | `rea xrefs game_stripped.exe 0x140004000` | Uso em `0x140002a65`, dentro da função principal |
| 4 | `rea function game_stripped.exe 0x140002a65` | Pseudocódigo da função principal e as funções que ela chama (52 s) |
| 5 | `rea function` em `0x140001450`, `0x1400014c0`, `0x140001480` | Pseudocódigo do pan, do passo do jogo e da colisão |

### O que foi recuperado

Função de pan do som, original:

```c
int sound_pan(int x, int screen_w) {
    int pan = (x * 200) / screen_w - 100;
    if (pan < -100) pan = -100;
    if (pan > 100) pan = 100;
    return pan;
}
```

O que o REA devolveu:

```c
int FUN_140001450(int param_1,int param_2)
{
  int iVar1;

  iVar1 = (param_1 * 200) / param_2 + -100;
  if (100 < iVar1) {
    iVar1 = 100;
  }
  if (iVar1 < -100) {
    iVar1 = -100;
  }
  return iVar1;
}
```

A função do passo do jogo voltou com a lógica correta, mas com a estrutura da bola desfeita em posições de memória:

```c
if ((int)CONCAT71(extraout_var,bVar1) != 0) {
  *(int *)((longlong)param_1 + 0xc) = -*(int *)((longlong)param_1 + 0xc);
  *(uint *)((longlong)param_1 + 0x14) =
       *(uint *)((longlong)param_1 + 0x14) + 10 + (*(uint *)((longlong)param_1 + 0x14) / 500) * 5;
}
```

No original, esse trecho é:

```c
if (hits(&r, paddle)) { b->dy = -b->dy; b->score += 10 + (b->score / 500) * 5; }
```

### Reescrita e conferência

O arquivo [recon.c](recon.c) foi escrito lendo apenas o pseudocódigo. Compilado e comparado com o `.exe` original:

| Passos do jogo (`n`) | Linhas de saída | Original e reescrito |
| --- | --- | --- |
| 0 | 0 | iguais |
| 1, 2, 99, 100 | 1 | iguais |
| 101 | 2 | iguais |
| 300 | 3 | iguais |
| 1.000 a 1.000.000 | 4 | iguais |

Doze execuções, doze saídas idênticas.

### O que se perdeu no caminho

- **Nomes**: `sound_pan` virou `FUN_140001450`, `x` virou `param_1`.
- **Estruturas**: `b->score` virou `*(param_1 + 0x14)`. Os campos tiveram de ser deduzidos pelas posições.
- **Forma do código**: `i % 100 == 0` voltou como `((uint)(iVar3 * -0x3d70a3d7) >> 2 | iVar3 * 0x40000000) < 0x28f5c29`, uma otimização do compilador. A leitura como "múltiplo de 100" foi uma inferência, confirmada pela comparação de saída.

### Limites deste teste

- O jogo acaba em menos de 400 passos e a bola nunca acerta a raquete. O cálculo de pontuação foi recuperado, mas **não foi exercitado** na comparação.
- São 4 funções de lógica. É uma prova de que o processo funciona, não de que ele escala.

## Testes complementares

| Tipo | Alvo | Resultado |
| --- | --- | --- |
| `.exe` nativo real | `unins000.exe` do VS Code, 3,4 MB | Falhou no prazo padrão (330 s). Com `REA_GHIDRA_STARTUP_TIMEOUT_MS=540000`, passou em 331 s: 8.904 funções, 8.666 strings |
| Recursos de `.exe` | o mesmo arquivo | 77 recursos e 5 grupos de ícones, cobertura completa, sem precisar de Ghidra |
| .NET | `System.Console.dll` | 45 tipos, 409 métodos, 1.042 chamadas; **sem código C#**, só assinaturas, contagem de instruções e quem chama quem |
| .NET | `System.Text.Json.dll`, 1,4 MB | Voltou vazio: metadados acima do limite interno de 64 MB |
| Jogo .NET (Steam) | `Stardew Valley.dll`, 6,3 MB, onde fica a lógica do jogo | Voltou vazio em 6 s, pelo mesmo limite de 64 MB: nenhum tipo, método ou chamada |
| JavaScript | extensão Git do VS Code, 1,1 MB | 71 módulos e 110 pontos de configuração/rede em 26 s; a análise profunda do `main.js` (765 KB) ficou sem memória, mesmo com 6 GB |
| JavaScript | VS Code inteiro, 5.993 arquivos | Não terminou: 3.252 arquivos em 587 s, quando o comando foi interrompido |

Em todos os casos de falha parcial, o REA marcou o resultado como parcial e informou o motivo.

## O que o REA faz e o que não faz

| Faz | Não faz |
| --- | --- |
| Acha funções a partir de textos e referências | Devolver o código-fonte original |
| Devolve pseudocódigo, assembly e quem chama quem | Escrever o código novo (quem escreve é o agente ou você) |
| Marca cada resultado como observado, inferido ou desconhecido | Compilar, portar ou emular |
| Lista módulos e dependências de apps JavaScript | Converter gráficos, sons e mapas de jogos |
| Lista tipos, métodos e chamadas de .NET | Decompilar .NET para C# |

Na parte nativa, quem decompila é o Ghidra. O REA é a camada que entrega esse resultado ao agente de forma organizada.

## Teste com jogo real em .NET: Stardew Valley

O REA voltou vazio na `Stardew Valley.dll`. O mesmo arquivo foi então aberto com o ILSpy (`ilspycmd` 11.1.0), um decompilador próprio para .NET.

| Medida | REA 6.4.0 | ILSpy 11.1.0 |
| --- | --- | --- |
| Tempo | 6 s | 45 s |
| Tipos recuperados | 0 | 1.137 |
| Arquivos de código | 0 | 948 arquivos `.cs` |
| Linhas de C# | 0 | 357.920 |
| Nomes originais | não se aplica | preservados |
| Recompila | não se aplica | quase: 12 erros em 357.920 linhas, em 27 s de build |

Trecho que saiu do ILSpy, sem nenhuma edição:

```csharp
public virtual void gainExperience(int which, int howMuch)
{
	if (which == 5 || howMuch <= 0)
	{
		return;
	}
	if (!IsLocalPlayer && Game1.IsServer)
	{
		queueMessage(17, Game1.player, which, howMuch);
		return;
	}
	if (Level >= 25)
	{
		int currentMasteryLevel = MasteryTrackerMenu.getCurrentMasteryLevel();
```

Compare com o que o Ghidra devolve para código nativo (`FUN_140001450`, `param_1`, `*(param_1 + 0x14)`): em .NET os nomes de classes, métodos e variáveis vêm prontos.

Os 12 erros de compilação são de três tipos (`CS0103`, `CS0026`, `CS0012`) e não foram corrigidos neste teste.

Conclusões deste teste:

- Para jogos .NET (e a maior parte dos jogos Unity), a leitura do código está praticamente resolvida pelo decompilador, sem REA.
- O agente recebe C# legível e trabalha direto nele; a etapa cara do caminho nativo (dar nomes e deduzir estruturas) não existe aqui.
- O REA 6.4.0 não é a ferramenta para esse tipo de alvo.

O código decompilado ficou só na pasta temporária da sessão, fora deste repositório.

## REA + agente: o que ainda falta

Hipótese de trabalho: o REA faz a leitura do binário e um agente (Claude ou outro) faz todo o resto, rodando por horas. Esta seção separa o que já foi testado do que é raciocínio a partir dos testes.

### Divisão do trabalho

| Etapa | Quem faz | Situação |
| --- | --- | --- |
| Achar as funções relevantes | REA (busca, referências) | Testado |
| Transformar em pseudocódigo | Ghidra, entregue pelo REA | Testado |
| Dar nomes, deduzir estruturas, reescrever | Agente | Testado em 4 funções |
| Compilar e comparar com o original | Agente | Testado, por saída de texto |
| Compilar para WebAssembly | Agente + Emscripten | Não testado |
| Traduzir gráficos, som e controles para o navegador | Agente | Não testado |
| Converter os arquivos do jogo | Agente | Não testado |

Nada nas três últimas linhas depende do REA: é código novo que o agente escreve. O que falta de verdade está abaixo.

### O que falta mesmo com REA + agente

**1. Ver o jogo original rodando.**
O REA, no Windows, só fez análise estática: ele lê o arquivo, não observa o programa em execução. Para um jogo, muita coisa só aparece rodando: valores calculados na hora, a ordem real dos eventos, o que cada posição de memória guarda. Falta um depurador ou gravador de execução (por exemplo, x64dbg) e um jeito de o agente usá-lo.

**2. Um jeito de comparar original e cópia.**
No teste, a comparação foi fácil porque o programa imprime texto. Um jogo produz imagem e som. Falta um mecanismo de conferência: gravar as mesmas entradas nos dois, capturar quadros e comparar. Sem isso o agente reescreve sem saber se acertou.

**3. Separar o código do jogo do código de biblioteca.**
O mini-jogo tinha 114 funções no `.exe`, e só 4 eram do jogo; o resto era o runtime do compilador. Num jogo real a proporção é parecida: boa parte é DirectX, compressão, áudio e runtime. Falta identificar o que é biblioteca conhecida, para o agente não gastar horas reconstruindo o que já existe pronto.

**4. Chamadas indiretas.**
O próprio REA avisou nos resultados: chamadas calculadas em tempo de execução não aparecem como referências. Jogos em C++ usam isso o tempo todo (métodos virtuais, tabelas de funções). O agente vai chegar a pontos em que a função chamada simplesmente não está indicada, e só o item 1 resolve.

**5. Executáveis protegidos.**
Se o `.exe` estiver compactado, ofuscado ou com proteção anticópia, o Ghidra enxerga a casca e não o jogo. O REA não desempacota. Falta uma etapa anterior para obter o binário limpo.

**6. Memória de longo prazo do agente.**
Em horas de trabalho o agente esquece o que já descobriu: nomes dados, estruturas deduzidas, funções prontas. Falta um registro persistente. O REA tem comandos para isso (`--snapshot`, `annotate-native-function`, `build-reconstruction-obligation-ledger`), mas não foram testados aqui.

**7. Velocidade das consultas.**
Pelo terminal, cada pergunta refez a análise inteira: 50 s no programa pequeno, 5,5 min no de 3,4 MB. Para milhares de funções isso inviabiliza. Falta uma sessão que mantenha o binário aberto, que é o papel do servidor MCP do REA; ele não conectou nesta máquina e precisa ser resolvido antes de qualquer teste longo.

**8. Os formatos dos arquivos do jogo.**
O agente consegue escrever conversores, mas só depois de entender o código que carrega cada formato, e isso volta para a leitura do binário. É trabalho a mais, não uma ferramenta que falte.

**9. O que não está no executável.**
Tudo que roda num servidor (modos online, contas, placares) não pode ser recuperado do `.exe`.

**10. Uma pessoa.**
Escolher por onde começar, jogar para sentir se a física ficou igual, decidir quando parar, e responder pela parte legal. O agente não julga "está igual ao original" pelo tato.

### Resumo

| Falta | Tipo |
| --- | --- |
| Observar o original rodando | Ferramenta que falta |
| Comparar imagem e som dos dois | Ferramenta que falta |
| Reconhecer bibliotecas | Configuração e método |
| Chamadas indiretas | Limite da análise estática |
| Executáveis protegidos | Etapa anterior |
| Memória entre sessões | Existe no REA, não testado |
| Sessão persistente (MCP) | Existe no REA, não funcionou aqui |
| Formatos de arquivo | Mais horas de agente |
| Parte online | Irrecuperável |
| Direção e julgamento | Pessoa |

Os dois primeiros itens são os que mais pesam: REA + agente recupera o que o programa **diz** que faz; falta ver o que ele **faz**.

### Escala

O desinstalador do VS Code tem 8.904 funções. Um jogo antigo pequeno tem alguns milhares. Com as lacunas acima resolvidas, um agente rodando por dias ou semanas dá conta de um jogo desse porte. O próximo teste útil é medir, num jogo real pequeno, quantas funções por hora saem reconstruídas e conferidas.

## Que tipo de jogo, que caminho

"Dá para qualquer jogo?" Não. O caminho e a dificuldade dependem de como o jogo foi feito.

| Tipo de jogo | Ferramenta de leitura | O que sai | Dificuldade | Situação |
| --- | --- | --- | --- | --- |
| .NET e a maior parte dos jogos Unity | ILSpy, dnSpy, dotPeek | C# legível com nomes originais | Baixa | Testado (Stardew Valley) |
| Nativo C/C++ pequeno | Ghidra + REA | Pseudocódigo sem nomes | Média, função por função | Testado (mini-jogo) |
| Nativo grande, com proteção ou anticheat | Ghidra + REA, depois de obter o binário limpo | Pseudocódigo sem nomes, em volume enorme | Muito alta | Não testado |
| Motor com formato próprio (ex.: jogos de "arrastar e soltar") | Ferramentas específicas do motor | Depende do motor | Baixa a média | Não testado |
| Código já aberto | Nenhuma | O próprio código | Nenhuma | — |

Exemplos discutidos, por raciocínio e não por teste:

| Jogo | Caso |
| --- | --- |
| Doom clássico | Código aberto; não precisa de engenharia reversa |
| FNAF | Motor com formato próprio |
| Rocket League | Nativo grande, com anticheat |
| Elden Ring | Nativo grande, com proteção; o mais difícil dos quatro |

## Mashups: juntam partes, não jogos

Hipótese de trabalho para os vídeos que misturam vários jogos (por exemplo, a jogabilidade de um no mundo de outro): ninguém funde os executáveis. Pega-se de cada jogo só a parte que interessa e monta-se tudo num motor só.

| Parte | De onde vem | Precisa de engenharia reversa? |
| --- | --- | --- |
| Mundo, modelos, texturas, sons | Extraídos dos arquivos do jogo com extratores do motor | Não do código; só do formato dos arquivos |
| Jogabilidade (física do carro, combate, câmera) | Recriada no motor novo, observando o original | Só se quiser o comportamento exato, número por número |
| Regras e números (dano, velocidade, tempos) | Lidos do código ou dos arquivos de dados | Sim, é onde a leitura do binário ajuda |
| Cola entre as partes | Código novo, escrito pelo agente | Não |

Consequências:

- A engenharia reversa vira ferramenta de consulta ("qual é a fórmula do impulso do carro?"), não o projeto inteiro. É exatamente o uso em que o REA foi bem no teste: uma função, uma resposta.
- O trabalho pesado passa a ser converter arquivos e fazer mecânicas de jogos diferentes conviverem (escala, física, controles).
- É um caminho muito mais curto do que decompilar qualquer um dos jogos por inteiro.

Nada desta seção foi testado ainda.

## O que precisa: máquina, espaço e agente

| Recurso | Necessidade | Máquina usada nos testes |
| --- | --- | --- |
| Processador e memória | PC comum para jogo pequeno ou médio; executável grande no Ghidra pede 16 GB ou mais e horas de análise | Ryzen 5 5600GT (6 núcleos), 32 GB |
| Disco | O código recuperado é pequeno (Stardew inteiro: 14 MB); o que ocupa são os jogos e os arquivos extraídos | 77 GB livres de 465 GB |
| Placa de vídeo | Só para rodar o resultado | Vídeo integrado |
| Ferramentas | Ghidra, ILSpy, dnSpy, dotPeek e REA são gratuitas | Todas instaladas |
| Agente | É o custo real: horas de agente rodando exigem plano pago | — |

O gargalo é tempo de agente, não a máquina.

## Usos além de jogos

Ideias levantadas na discussão. Nenhuma foi testada ainda; a coluna "Viável aqui" diz o que já roda na máquina dos testes.

| Uso | O que se faz | Por que importa | Viável aqui |
| --- | --- | --- | --- |
| Resgate de sistema legado | Recuperar regras de negócio e formato dos dados de um programa antigo sem código-fonte (Delphi, VB6, C++), para migrar | Clínicas, prefeituras e pequenas empresas presas a programas cujo desenvolvedor sumiu | Sim: Ghidra + REA; o `.exe` real testado era feito em Delphi |
| Privacidade de apps de saúde | Ver para onde um app ou site de saúde envia dados, lendo as requisições de rede e o código | Dados de saúde são sensíveis e o usuário não tem como conferir | Parcial: site funciona com um navegador Chrome; app Android pede JADX |
| Equipamento médico ou de laboratório antigo | Recuperar o formato de arquivo de um aparelho descontinuado para converter exames para um padrão aberto | Dados de pacientes presos em formatos que nada mais lê | Sim, se houver o programa do aparelho |
| Direito ao reparo | Ler o firmware de um aparelho que perdeu suporte para mantê-lo funcionando | Evita descarte de equipamento que ainda funciona | Não: análise de firmware do REA só roda em Linux |
| Segurança | Procurar falhas em programas e identificar o que um arquivo suspeito faz | Defesa | Sim: Ghidra + REA |
| Preservação digital | Documentar formatos e programas abandonados antes que se percam | Memória técnica e cultural | Sim |
| Ensino | Mostrar como código vira instruções de máquina e volta | Organização de computadores, compiladores | Sim: o mini-jogo deste estudo já serve de aula |

### Ideia principal: resgate de sistema legado

O uso mais promissor, na avaliação feita na conversa, é o resgate de sistema legado.

**O problema.** Clínicas, prefeituras e pequenas empresas dependem de programas antigos (Delphi, VB6, C++) cujo desenvolvedor sumiu. Não há código-fonte, o programa só roda numa máquina velha e os dados estão presos num formato que nada mais lê.

**O que se faria.**

1. Abrir o executável com Ghidra + REA e localizar as funções que leem e gravam os dados.
2. Recuperar o formato dos arquivos ou do banco e as regras de negócio (cálculos, validações).
3. O agente escreve um conversor dos dados antigos para um formato aberto e documenta as regras.
4. Conferir: os mesmos registros, lidos pelo programa antigo e pelo conversor, têm de bater.

**Por que é uma boa aposta.**

- O problema é comum e caro de resolver do jeito tradicional.
- O alvo costuma ser pequeno, da escala em que REA + Ghidra funcionou nos testes.
- Quem pede o trabalho é o dono do programa, sem questão de direitos de terceiros.
- O resultado tem valor prático imediato: os dados voltam a ser usáveis.
- Na saúde, vira migração de prontuários e exames de um sistema antigo de clínica para um atual.

**O que falta para testar.** Um programa antigo de verdade, com dados de exemplo. O `.exe` real analisado neste estudo (um desinstalador) era feito em Delphi e abriu sem problema, mas não é um sistema com dados.

## Teste com site: editor de slides do Canva

Feito em 2026-10-11. A ideia era apontar o REA para `canva.com` e trazer a funcionalidade de criar slides para outro projeto.

### Tentativa 1: navegador automático, sem login

`capture_browser_scenario` abrindo `https://www.canva.com/create/presentations/` num Chrome isolado (perfil temporário, sem janela).

**Resultado.** O Canva respondeu 307 e depois **403**, com a página de bloqueio do Cloudflare (desafio Turnstile; título "Você vai poder voltar a criar designs em breve"). Nenhum código do editor chegou a carregar. O bloqueio não foi contornado.

### Tentativa 2: observação passiva, com o usuário logado

Chrome aberto com `--remote-debugging-port=9222 --user-data-dir=C:\dev\chrome-rea` (perfil separado). O usuário fez o login e abriu o editor; o REA só observou a aba.

**Funcionou.** Com uma pessoa de verdade no navegador, o Cloudflare não barrou. Ferramentas usadas: `list_browser_targets`, `inspect_web_page` (com `include_accessibility_text`) e `capture_web_screenshot`.

**O que o REA entrega nesse modo.** A árvore de acessibilidade da página: todos os controles visíveis, com papel, nome e estado. Junto com a foto da tela, isso dá o inventário da interface. O que foi lido:

| Estado na tela | O que apareceu |
| --- | --- |
| Editor vazio (A4) | Barra de cima (Home, File, Resize, Editing, salvo, título, Share), abas do painel lateral, grade de templates, Add page, rodapé (Notes, Timer, zoom, Pages, Grid view, tela cheia) |
| Apresentação, painel Shapes | Categorias com "See all" (Lines, Basic shapes, Polygons, Stars, Arrows, Flowchart, Speech bubbles...), faixa de miniaturas com "+" e menu de tipo de página, botão Present com menu de modos |
| Texto selecionado | Barra: fonte, tamanho com − e +, cor, negrito, itálico, sublinhado, tachado, maiúsculas, alinhamento, lista, espaçamento, transparência, Effects, Animate, Position. 6 alças (cantos e laterais) e girar embaixo. Barrinha flutuante |
| Quadrado selecionado | Barra: Edit, cor (cinza `#919191`), borda, cantos, transparência, Effects, Animate, Position. 8 alças. Barrinha: Comment, Lock, Duplicate, Delete, More |
| Imagem selecionada | Barra: Edit, Replace, BG Remover, Eraser, cor, borda, cantos, Crop, Flip, transparência, Animate, Position |
| Painel Uploads | Busca, Upload files, abas Images/Videos/Designs/Folders/Files, grade com as imagens enviadas |

**Limites encontrados.**

- Só aparece o que está na tela. Cada estado (texto selecionado, forma selecionada...) precisou que o usuário deixasse a tela daquele jeito e avisasse. Janela do Windows (escolher arquivo) fica de fora.
- O modo é passivo: o REA não clica. O ritmo depende de quem está no navegador.
- As respostas são grandes (de 280 mil a 1 milhão de caracteres por captura). Foi preciso filtrar o JSON por script antes de ler.
- O que sai é a estrutura da interface, não a lógica interna. O código do editor não foi lido nem copiado; é minificado e do Canva.

### O que foi construído

Um editor de slides escrito do zero em `C:\dev\rea\results\canva\slide-creator` (JavaScript puro, sem dependências), em duas voltas:

1. Antes do login: feito só com o que se sabe de editores desse tipo.
2. Depois da observação: interface refeita seguindo o inventário acima (barra de propriedades por tipo, barrinha flutuante, alças, painel de formas por categoria, Uploads, Posição, rodapé com zoom/páginas/grade, bloqueio, menu de contexto).

Conferido com um teste automático no Chrome (mouse e teclado de verdade), sem erros no console.

**O que não veio e não dá para trazer assim:** templates, fotos, kit de marca, efeitos, animações, recorte, remoção de fundo, apps de IA, comentários e compartilhamento. Parte é conteúdo do Canva, parte roda no servidor deles.

**Conclusão deste teste.** Para "clonar" uma funcionalidade de site, o REA serviu como olho: mostra com precisão o que existe na tela e em que estado. Quem reescreve é o agente. O resultado se parece no uso, não é uma cópia do original.

## Teste com site: Overleaf

Começado em 2026-10-11. Objetivo: recriar o site tela por tela, começando pelo que aparece antes do login.

### Página inicial (sem login)

Mesma técnica do Canva: o usuário abriu `overleaf.com` no Chrome de depuração e o REA observou (`inspect_web_page` com texto de acessibilidade + `capture_web_screenshot`).

**O que veio.** Como é uma página de conteúdo, e não um aplicativo, uma única captura trouxe a página inteira, de cima a baixo, mesmo com só o topo visível na tela: menu, formulário de cadastro, todos os títulos e listas das seções, planos, rodapé com seis colunas e o aviso de cookies. Foram 898 nós de acessibilidade, que viraram 383 linhas úteis.

**O que não veio.**

- Os itens dentro dos menus suspensos (Product, Solutions, Help & resources): estavam fechados, então não existem na árvore.
- A aparência abaixo da primeira dobra: a foto é só do que está visível, e o REA não rola a página.
- Vídeos e imagens: só o texto alternativo.

**Diferença para o Canva.** Página de conteúdo se lê de uma vez. Aplicativo (editor) só mostra o estado atual e precisa de uma captura por estado.

**O que foi construído.** `C:\dev\rea\results\overleaf\index.html`: página com as mesmas seções e o mesmo comportamento (menus, cadastro com validação, cookies, título animado), com nome, textos e ilustrações próprios. Não foram copiados logotipo, textos de marketing, logotipos de clientes nem depoimentos. Teste automático no Chrome passou, em tela grande e de celular.

### Modo automático: o REA navegando sozinho

Depois de abrir tela por tela à mão, o usuário perguntou se o REA não podia fazer isso sozinho. Pode: `capture_browser_scenario` em modo `launch` abre um Chrome isolado, segue uma lista de ações (`goto`, `click`, `wait`) e devolve, a cada passo, o endereço, a foto e o texto de acessibilidade.

**Resultado.** O Overleaf não barrou o navegador automático (o Canva tinha barrado). Em três rodadas, de 8 a 10 segundos cada, foram lidas 22 páginas públicas: início, preços, login, cadastro, redefinir senha, SSO, recursos, IA, por que LaTeX, detalhe de modelo, as quatro de soluções, histórias de clientes, sobre, contato, blog, aprenda e termos.

**Diferenças para o modo passivo.**

- O texto de acessibilidade vem pronto e legível (formato de árvore, com o endereço de cada link). No modo passivo vinha como lista de nós e precisava de script para montar.
- Dá para clicar em menus (o item "AI" foi achado clicando em Product, sem saber o endereço).
- Uma rodada com 10 páginas e fotos passa de 2 milhões de caracteres; continua sendo preciso separar por passo em arquivos.

**Comparando os três jeitos testados.**

| Jeito | Canva | Overleaf |
| --- | --- | --- |
| Automático, navegador isolado | Barrado (403) | Funcionou |
| Passivo, usuário logado | Funcionou | Funcionou |
| Automático na aba do usuário | Não testado | Autorizado, ainda não usado |

**O que foi construído nesta rodada.** Em `C:\dev\rea\results\overleaf`: galeria de modelos com página de categoria e de detalhe, preços (chaves anual/estudante, licenças de grupo, tabela comparativa), login, cadastro, redefinição de senha e SSO. Preços, modelos e textos são próprios. Testes automáticos passaram em tela grande e de celular.

**Regras combinadas.** O usuário autorizou o uso de uma aba própria no Chrome dele, com o login sempre feito por ele. As regras ficaram em `C:\dev\rea\results\RULES.md`.

### Fechando a parte pública

**Compilador.** A máquina não tinha LaTeX. Foi instalado o Tectonic 0.17.0 (um executável só, em `C:\tools\tectonic`), com autorização do usuário. Um servidor em Node recebe o `.tex` e devolve o PDF. Medido aqui:

| Compilação | Tempo |
| --- | --- |
| Primeira de todas (baixa o pacote básico) | 2 min 14 s |
| Primeira com Beamer (baixa a classe) | 83 s |
| Demais, primeira vez de cada documento | 0,5 a 4 s |
| Repetida (cache) | 1 ms |

Os 12 tipos de documento de exemplo compilam. Erro de LaTeX volta como resposta 422 com o log.

**Páginas de conteúdo.** As 13 que faltavam foram geradas a partir da estrutura lida pelo REA: recursos, IA, por que LaTeX, quatro de soluções, histórias, sobre, contato, blog, aprenda e termos. Total: 21 páginas, 58 links internos conferidos por teste.

**O que a leitura da estrutura não resolve.** Em página de produto, quase tudo é conteúdo do dono: números, clientes, depoimentos, certificações, publicações, texto jurídico. O REA mostra onde cada coisa fica; o conteúdo em si não se recria. Essas partes ficaram como marcadores. Blog, central de ajuda e termos são só esqueleto.

**Próximo passo.** Com o usuário logado: lista de projetos e editor.

**Observação.** O código do editor do Overleaf é publicado como software livre (Community Edition, AGPL v3). Para a parte do editor, ler o código publicado é mais direto do que observar a tela.

## Plano: material em português

O repositório do REA já tem um `README_pt-BR.md`, tradução do original. O que não existe é um material em português baseado em teste: o que funciona de verdade, em Windows, com números.

Proposta discutida:

- repositório separado, só com o estudo de caso e um guia prático, apontando para o projeto original;
- conteúdo: para que serve, como instalar (REA, Ghidra, ILSpy), como usar, exemplos testados, limites encontrados, usos além de jogos;
- deixar claro que é um estudo de caso independente, sem vínculo com os autores do REA;
- não publicar código decompilado de jogos comerciais; só os arquivos de teste próprios (`game.c`, `recon.c`) e os números.

## Conclusão

1. O REA, com o Ghidra, recupera a lógica de um `.exe` nativo sem código-fonte de forma utilizável. Isso foi medido, em escala pequena.
2. O ganho real é para um agente: ele faz buscas, segue referências e lê funções sem intervenção manual.
3. Fora do nativo, a versão 6.4.0 entregou menos do que a documentação sugere: mapa sem lógica completa em JavaScript, estrutura sem código em .NET.
4. A velocidade pelo terminal é baixa, porque cada consulta refaz a análise: cerca de 50 s no programa pequeno e 5,5 min no de 3,4 MB.
5. REA + agente cobre a lógica. Para um jogo completo faltam, principalmente, observar o original em execução e comparar imagem e som entre original e cópia.
6. O que foi demonstrado: "com REA + Ghidra, um agente recuperou e reescreveu a lógica de um executável sem código-fonte, e a saída bateu com a original".

## Como repetir

```bash
export GHIDRA_INSTALL_DIR='C:\tools\ghidra_12.1.4_PUBLIC'

gcc -O2 -s -o game_stripped.exe game.c

npx -y rea-agents@latest analyze  game_stripped.exe
npx -y rea-agents@latest search   game_stripped.exe score
npx -y rea-agents@latest xrefs    game_stripped.exe 0x140004000
npx -y rea-agents@latest function game_stripped.exe 0x140002a65

gcc -O2 -o recon.exe recon.c
./game_stripped.exe 1000 > a.txt
./recon.exe 1000 > b.txt
diff a.txt b.txt
```

Os endereços podem mudar com outra versão do compilador.
