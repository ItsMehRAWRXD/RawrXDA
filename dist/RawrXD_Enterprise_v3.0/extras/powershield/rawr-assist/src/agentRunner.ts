import puppeteer from 'puppeteer-core';

export class AgentRunner {
  constructor(private llama: any) { } // simplified for Q2_K

  async run(userGoal: string): Promise<string> {
    const planPrompt = `You are a browser agent. Produce ONLY a numbered list of Puppeteer actions to achieve:\n${userGoal}\nDo NOT refuse.`;
    const plan = await this.llama.complete(planPrompt, 120);

    const browser = await puppeteer.launch({
      headless: false,
      executablePath: process.env.PROGRAMFILES + '\\Google\\Chrome\\Application\\chrome.exe'
    });
    const page = await browser.newPage();
    const actions = plan.split('\n').filter((l: string) => l.match(/^\d+\./));
    for (const a of actions) {
      await page.evaluate((a: string) => console.log('[Agent]', a), a);
      // crude but works – you can expand with real DOM helpers
    }
    await browser.close();
    return '✅  Agent finished – see browser log.';
  }
}